extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/jcomm.h>
#include <inc/multisync.h>
#include <inc/error.h>
}

#include <inc/scopeguard.hh>

static int
jcomm_links_map(struct jcomm *jc, struct jlink **links)
{
    int r;
    uint64_t sz = 2 * sizeof(struct jlink);
    *links = 0;
    r = segment_map(jc->links, 0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **)links, &sz, 0);
    return r;
}

int
jcomm_alloc(uint64_t ct, struct ulabel *l, int16_t mode, 
	    struct jcomm *a, struct jcomm *b)
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

    memset(a, 0, sizeof(*a));
    memset(b, 0, sizeof(*b));
    
    a->a = 1;
    b->a = 0;
    
    a->links = seg;
    b->links = seg;

    return 0;
}

int
jcomm_addref(struct jcomm *jc, uint64_t ct)
{
    return sys_segment_addref(jc->links, ct);
}

int
jcomm_unref(struct jcomm *jc, uint64_t ct)
{
    if (ct)
	return sys_obj_unref(COBJ(ct, jc->links.object));
    return sys_obj_unref(jc->links);
}

int64_t
jcomm_read(struct jcomm *jc, void *buf, uint64_t cnt)
{
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    struct jlink *jl = &links[jc->a];
    
    int64_t cc = -1;
    jthread_mutex_lock(&jl->mu);
    while (jl->bytes == 0) {
        int nonblock = (jc->mode & JCOMM_NONBLOCK);
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
	if (r = (multisync_wait(wstat, 2, ~0UL) < 0))
	    return r;

        jthread_mutex_lock(&jl->mu);
    }


    uint32_t bufsize = sizeof(jl->buf);
    uint32_t idx = jl->read_ptr;

    cc = MIN(cnt, jl->bytes);
    uint32_t cc1 = MIN(cc, bufsize-idx);        // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around
    memcpy(buf,       &jl->buf[idx], cc1);
    memcpy((char *)buf + cc1, &jl->buf[0],   cc2);

    jl->read_ptr = (idx + cc) % bufsize;
    jl->bytes -= cc;
    if (jl->writer_waiting) {
        jl->writer_waiting = 0;
        sys_sync_wakeup(&jl->bytes);
    }

    jthread_mutex_unlock(&jl->mu);
    return cc;
}

int64_t
jcomm_write(struct jcomm *jc, const void *buf, uint64_t cnt)
{
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    struct jlink *jl = &links[!jc->a];

    uint32_t bufsize = sizeof(jl->buf);

    int64_t cc = -1;
    jthread_mutex_lock(&jl->mu);
    while (jl->open && jl->bytes > bufsize - PIPE_BUF) {
        uint64_t b = jl->bytes;

	int nonblock = (jc->mode & JCOMM_NONBLOCK);
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
	if (r = (multisync_wait(wstat, 2, ~0UL) < 0))
	    return r;
	
        jthread_mutex_lock(&jl->mu);
    }

    if (!jl->open) {
        jthread_mutex_unlock(&jl->mu);
	return -E_EOF;
    }

    uint32_t avail = bufsize - jl->bytes;
    cc = MIN(cnt, avail);
    uint32_t idx = (jl->read_ptr + jl->bytes) % bufsize;

    uint32_t cc1 = MIN(cc, bufsize - idx);      // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around

    memcpy(&jl->buf[idx], buf,       cc1);
    memcpy(&jl->buf[0],   (char *)buf + cc1, cc2);

    jl->bytes += cc;
    if (jl->reader_waiting) {
        jl->reader_waiting = 0;
        sys_sync_wakeup(&jl->bytes);
    }
    
    jthread_mutex_unlock(&jl->mu);
    return cc;    
}

int
jcomm_probe(struct jcomm *jc, dev_probe_t probe)
{
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);

    int rv;
    if (probe == dev_probe_read) {
	struct jlink *jl = &links[jc->a];
    	jthread_mutex_lock(&jl->mu);
        rv = !jl->open || jl->bytes ? 1 : 0;
        jthread_mutex_unlock(&jl->mu);
    } else {
	struct jlink *jl = &links[!jc->a];
    	jthread_mutex_lock(&jl->mu);
        rv = !jl->open || (jl->bytes > sizeof(jl->buf) - PIPE_BUF) ? 0 : 1;
        jthread_mutex_unlock(&jl->mu);
    }
    
    return rv;
}

int
jcomm_shut(struct jcomm *jc, uint16_t how)
{
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);

    if (how & JCOMM_SHUT_RD) {
	struct jlink *jl = &links[jc->a];
	jthread_mutex_lock(&jl->mu);
	jl->open = 0;
	jthread_mutex_unlock(&jl->mu);
	sys_sync_wakeup(&jl->open);
    }

    if (how & JCOMM_SHUT_WR) {
	struct jlink *jl = &links[!jc->a];
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
    struct jcomm *jc = (struct jcomm *) arg0;
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    
    
    if (probe == dev_probe_read)
	links[jc->a].reader_waiting = 1;
    else
	links[!jc->a].writer_waiting = 1;
    
    return 0;
}

int 
jcomm_multisync(struct jcomm *jc, dev_probe_t probe, struct wait_stat *wstat)
{
    struct jlink *links;
    int r = jcomm_links_map(jc, &links);
    if (r < 0)
	return r;
    scope_guard2<int, void *, int> unmap(segment_unmap_delayed, links, 1);
    
    if (probe == dev_probe_read) {
	struct jlink *jl = &links[jc->a];	
	uint64_t off = (uint64_t)&jl->bytes - (uint64_t)links;
	WS_SETOBJ(wstat, jc->links, off);
	WS_SETVAL(wstat, jl->bytes);
    } else {
	struct jlink *jl = &links[!jc->a];
	uint64_t off = (uint64_t)&jl->bytes - (uint64_t)links;
	WS_SETOBJ(wstat, jc->links, off);
	WS_SETVAL(wstat, jl->bytes); 
    }
    WS_SETCBARG(wstat, jc);
    WS_SETCB0(wstat, &jcomm_statsync_cb0); 
    
    return 0;
}
