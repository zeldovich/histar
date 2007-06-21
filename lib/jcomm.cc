extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/jcomm.h>
#include <inc/multisync.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <malloc.h>
#include <inttypes.h>
}

#include <inc/scopeguard.hh>

#define JCSEG(jr) (COBJ(jr.container, jr.jc.segment))

static char
jlink_minwrite(struct jlink *jl)
{
    return !(jl->bytes > (sizeof(jl->buf) - PIPE_BUF));
}

static char
jlink_fullwrite(struct jlink *jl, uint64_t cnt)
{
    return !(cnt > (sizeof(jl->buf) - jl->bytes));
} 

static uint64_t
jlink_copyfrom(void *buf, struct jlink *jl, uint64_t cnt)
{
    uint64_t bufsize = sizeof(jl->buf);
    uint64_t idx = jl->read_ptr;
    
    uint64_t cc1 = MIN(cnt, bufsize-idx);        // idx to end-of-buffer
    uint64_t cc2 = (cc1 == cnt) ? 0 : (cnt - cc1);    // wrap-around
    memcpy(buf,       &jl->buf[idx], cc1);
    memcpy((char *)buf + cc1, &jl->buf[0],   cc2);

    return (idx + cnt) % bufsize;
}

static void
jlink_copyto(struct jlink *jl, const void *buf, uint64_t cnt)
{
    uint64_t bufsize = sizeof(jl->buf);
    uint64_t idx = (jl->read_ptr + jl->bytes) % bufsize;

    uint64_t cc1 = MIN(cnt, bufsize - idx);      // idx to end-of-buffer
    uint64_t cc2 = (cc1 == cnt) ? 0 : (cnt - cc1);    // wrap-around

    memcpy(&jl->buf[idx], buf,       cc1);
    memcpy(&jl->buf[0],   (char *)buf + cc1, cc2);
}

int64_t
jlink_read(struct jlink *jl, void *buf, uint64_t cnt, int16_t mode)
{
    int r;
    int64_t cc = -1;
    jthread_mutex_lock(&jl->mu);
    while (jl->bytes == 0) {
        int nonblock = (mode & JCOMM_NONBLOCK_RD);
	if (!nonblock)
	    jl->reader_waiting = 1;

        char opn = jl->open;
        jthread_mutex_unlock(&jl->mu);

        if (!opn)
	    return 0;
    
        if (nonblock)
	    return -E_AGAIN;

	struct wait_stat wstat[2];
	memset(wstat, 0, sizeof(wstat));
	WS_SETADDR(&wstat[0], &jl->bytes);
	WS_SETVAL(&wstat[0], 0);
	WS_SETADDR(&wstat[1], &jl->open);
	WS_SETVAL(&wstat[1], 1);
	if ((r = multisync_wait(wstat, 2, UINT64(~0))) < 0)
	    return r;

        jthread_mutex_lock(&jl->mu);
    }

    if (mode & JCOMM_PACKET) {
	uint64_t header;
	uint64_t read_ptr = jlink_copyfrom(&header, jl, sizeof(header));
	if (header > cnt)
	    return -E_NO_SPACE;
	jl->read_ptr = read_ptr;
	jl->bytes -= sizeof(header);
	
	jl->read_ptr = jlink_copyfrom(buf, jl, header);
	jl->bytes -= header;
	cc = header;
    } else {
	cc = MIN(cnt, jl->bytes);	
	jl->read_ptr = jlink_copyfrom(buf, jl, cc);
	jl->bytes -= cc;	
    }

    if (jl->writer_waiting) {
        jl->writer_waiting = 0;
        sys_sync_wakeup(&jl->bytes);
    }

    jthread_mutex_unlock(&jl->mu);
    return cc;
}

int64_t
jlink_write(struct jlink *jl, const void *buf, uint64_t cnt, int16_t mode)
{
    int r;
    uint64_t header = cnt;
    uint64_t pm = mode & JCOMM_PACKET;
    uint64_t bufsize = sizeof(jl->buf);

    if (pm && cnt > bufsize) {
	cprintf("jlink_write: req. write too big: %"PRIu64" > %"PRIu64"\n",
		cnt, bufsize);
	return -E_INVAL;
    }
    
    jthread_mutex_lock(&jl->mu);
    while ((jl->open) && 
	   ((pm && !jlink_fullwrite(jl, cnt + sizeof(header))) ||
	    (!pm && !jlink_minwrite(jl))))
    {
    	uint64_t b = jl->bytes;
	
	int nonblock = (mode & JCOMM_NONBLOCK_WR);
	if (!nonblock)
	    jl->writer_waiting = 1;

        jthread_mutex_unlock(&jl->mu);

	if (nonblock)
	    return -E_AGAIN;

	struct wait_stat wstat[2];
	memset(wstat, 0, sizeof(wstat));
	WS_SETADDR(&wstat[0], &jl->bytes);
	WS_SETVAL(&wstat[0], b);
	WS_SETADDR(&wstat[1], &jl->open);
	WS_SETVAL(&wstat[1], 1);
	if ((r = multisync_wait(wstat, 2, UINT64(~0))) < 0)
	    return r;
	
        jthread_mutex_lock(&jl->mu);
    }

    if (!jl->open) {
        jthread_mutex_unlock(&jl->mu);
	return -E_EOF;
    }

    uint64_t cc;
    if (pm) {
	jlink_copyto(jl, &header, sizeof(header));
	jl->bytes += sizeof(header);
	jlink_copyto(jl, buf, cnt);
	jl->bytes += cnt;
	cc = cnt;
    } else {
	uint64_t avail = bufsize - jl->bytes;
	cc = MIN(cnt, avail);
	jlink_copyto(jl, buf, cc);
	jl->bytes += cc;
    }

    if (jl->reader_waiting) {
        jl->reader_waiting = 0;
        sys_sync_wakeup(&jl->bytes);
    }
    
    jthread_mutex_unlock(&jl->mu);
    return cc;
}

static int
jcomm_links_map(struct jcomm_ref jr, struct jlink **links)
{
    int r;
    uint64_t sz = 2 * sizeof(struct jlink);
    *links = 0;
    r = segment_map(JCSEG(jr), 0, SEGMAP_READ | SEGMAP_WRITE, 
		    (void **)links, &sz, 0);
    return r;
}

int
jcomm_alloc(uint64_t ct, struct ulabel *l, int16_t mode, 
	    struct jcomm_ref *a, struct jcomm_ref *b)
{
    int r;
    uint64_t sz = 2 * sizeof(struct jlink);
    
    struct cobj_ref seg;
    struct jlink *links = 0;
    r = segment_alloc(ct, sz, &seg, (void **)&links, l, "jcomm-seg");
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    
    r = sys_obj_set_fixedquota(seg);
    if (r < 0)
	return r;
    
    memset(links, 0, sz);
    links[0].open = 1;
    links[1].open = 1;

    links[0].mode = mode;
    links[1].mode = mode;

    memset(a, 0, sizeof(*a));
    memset(b, 0, sizeof(*b));
    
    a->jc.chan = jcomm_chan0;
    b->jc.chan = jcomm_chan1;
    a->jc.segment = seg.object;
    b->jc.segment = seg.object;

    a->container = seg.container;
    b->container = seg.container;

    return 0;
}

int 
jcomm_mode_set(struct jcomm_ref jr, int16_t mode)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    struct jlink *jl = &links[jr.jc.chan];
    jl->mode = mode;
    return 0;
}

int
jcomm_nonblock_enable(struct jcomm_ref jr)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    
    links[jr.jc.chan].mode |= JCOMM_NONBLOCK_RD;
    links[!jr.jc.chan].mode |=  JCOMM_NONBLOCK_WR;
    
    return 0;
}

int
jcomm_addref(struct jcomm_ref jr, uint64_t ct)
{
    return sys_segment_addref(JCSEG(jr), ct);
}

int
jcomm_unref(struct jcomm_ref jr)
{
    return sys_obj_unref(JCSEG(jr));
}

int64_t
jcomm_read(struct jcomm_ref jr, void *buf, uint64_t cnt)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    struct jlink *jl = &links[jr.jc.chan];

    return jlink_read(jl, buf, cnt, jl->mode);
}

int64_t
jcomm_write(struct jcomm_ref jr, const void *buf, uint64_t cnt)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    struct jlink *jl = &links[!jr.jc.chan];

    return jlink_write(jl, buf, cnt, jl->mode);
}

int
jcomm_probe(struct jcomm_ref jr, dev_probe_t probe)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);

    int rv;
    if (probe == dev_probe_read) {
	struct jlink *jl = &links[jr.jc.chan];
    	jthread_mutex_lock(&jl->mu);
        rv = !jl->open || jl->bytes ? 1 : 0;
        jthread_mutex_unlock(&jl->mu);
    } else {
	struct jlink *jl = &links[!jr.jc.chan];
    	jthread_mutex_lock(&jl->mu);
        rv = !jl->open || (jl->bytes > sizeof(jl->buf) - PIPE_BUF) ? 0 : 1;
        jthread_mutex_unlock(&jl->mu);
    }
    
    return rv;
}

int
jcomm_shut(struct jcomm_ref jr, uint16_t how)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);

    if (how & JCOMM_SHUT_RD) {
	struct jlink *jl = &links[jr.jc.chan];
	jthread_mutex_lock(&jl->mu);
	jl->open = 0;
	jthread_mutex_unlock(&jl->mu);
	sys_sync_wakeup(&jl->open);
    }

    if (how & JCOMM_SHUT_WR) {
	struct jlink *jl = &links[!jr.jc.chan];
	jthread_mutex_lock(&jl->mu);
	jl->open = 0;
	jthread_mutex_unlock(&jl->mu);
	sys_sync_wakeup(&jl->open);
    }
    
    return 0;
}

static int
jcomm_statsync_cb0(void *arg0, dev_probe_t probe, volatile uint64_t *addr, 
		    void **arg1)
{
    struct jcomm_ref *jr = (struct jcomm_ref *) arg0;
    struct jlink *links;
    int r = jcomm_links_map(*jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    
    if (probe == dev_probe_read)
	links[jr->jc.chan].reader_waiting = 1;
    else
	links[!jr->jc.chan].writer_waiting = 1;

    free(jr);
    return 0;
}

int 
jcomm_multisync(struct jcomm_ref jr, dev_probe_t probe, struct wait_stat *wstat)
{
    struct jlink *links;
    int r = jcomm_links_map(jr, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    memset(wstat, 0, sizeof(*wstat));

    if (probe == dev_probe_read) {
	struct jlink *jl = &links[jr.jc.chan];	
	uint64_t off = (uintptr_t)&jl->bytes - (uintptr_t)links;
	WS_SETOBJ(wstat, COBJ(jr.container, jr.jc.segment), off);
	WS_SETVAL(wstat, jl->bytes);
    } else {
	struct jlink *jl = &links[!jr.jc.chan];
	uint64_t off = (uintptr_t)&jl->bytes - (uintptr_t)links;
	WS_SETOBJ(wstat, COBJ(jr.container, jr.jc.segment), off);
	WS_SETVAL(wstat, jl->bytes); 
    }

    struct jcomm_ref *jr_copy = (struct jcomm_ref *)malloc(sizeof(jr));
    if (!jr_copy)
	return -E_NO_MEM;
    memcpy(jr_copy, &jr, sizeof(*jr_copy));
    
    WS_SETCBARG(wstat, jr_copy);
    WS_SETCB0(wstat, &jcomm_statsync_cb0); 
    wstat->ws_probe = probe;
    return 0;
}
