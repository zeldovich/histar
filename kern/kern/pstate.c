#include <machine/pmap.h>
#include <machine/thread.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <dev/disk.h>
#include <kern/disklayout.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/log.h>
#include <inc/error.h>

// verbose flags
static int pstate_load_debug = 0;
static int pstate_swapin_debug = 0;
static int pstate_swapout_debug = 0;
static int pstate_swapout_stats = 0;

static int scrub_disk_pages = 0;

// Authoritative copy of the header that's actually on disk.
static struct pstate_header stable_hdr;

// Global freelist for disk
struct freelist freelist;

struct mobject {
    offset_t off;
    uint64_t nbytes;
};

// Scratch-space for a copy of the header used while reading/writing.
#define PSTATE_BUF_SIZE		PGSIZE
static union {
    struct pstate_header hdr;
    char buf[PSTATE_BUF_SIZE];
} pstate_buf;

//////////////////////////////////////////////////
// Object map
//////////////////////////////////////////////////

static void
pstate_kobj_free(struct freelist *f, struct kobject *ko)
{
    uint64_t key;
    struct mobject mobj;
    
    int r = btree_search(BTREE_OBJMAP, &ko->hdr.ko_id, &key, (uint64_t *)&mobj);
    if (r == 0) {
    	assert(key == ko->hdr.ko_id);

	if (scrub_disk_pages) {
	    disk_io_status s;
	    void *p;
	    assert(0 == page_alloc(&p));
	    memset(p, 0xc4, PGSIZE);

	    for (uint32_t i = 0; i < mobj.nbytes; i += 512)
		s = stackwrap_disk_io(op_write, p, 512, mobj.off + i * 512);

	    page_free(p);
	}

	freelist_free_later(f, mobj.off, mobj.nbytes);
	assert(0 == btree_delete(BTREE_OBJMAP, &ko->hdr.ko_id));

	r = btree_delete(BTREE_IOBJ, &ko->hdr.ko_id);
	assert(r == 0 || r == -E_NOT_FOUND);
    }
}

static int64_t
pstate_kobj_alloc(struct freelist *f, struct kobject *ko)
{
    int r;
    pstate_kobj_free(f, ko);

    uint64_t nbytes = KOBJ_DISK_SIZE + ROUNDUP(ko->hdr.ko_nbytes, 512);
    int64_t offset = freelist_alloc(f, nbytes);

    if (offset < 0) {
	cprintf("pstate_kobj_alloc: no room for %ld bytes\n", nbytes);
	return offset;
    }

    struct mobject mobj = { offset, nbytes };
    r = btree_insert(BTREE_OBJMAP, &ko->hdr.ko_id, (uint64_t *)&mobj);
    if (r < 0) {
	cprintf("pstate_kobj_alloc: objmap insert failed, disk full?\n");
	return r;
    }

    if (kobject_initial(ko)) {
	uint64_t dummy = 0;
	r = btree_insert(BTREE_IOBJ, &ko->hdr.ko_id, &dummy);
	if (r < 0) {
	    cprintf("pstate_kobj_alloc: iobjlist insert failed, disk full?\n");
	    return r;
	}
    }
    
    return offset;
}

//////////////////////////////////////////////////
// Scatter-gather buffering logic
//////////////////////////////////////////////////

struct pstate_iov_collector {
    struct iovec iov_buf[DISK_REQMAX / PGSIZE];
    uint32_t iov_cnt;
    uint32_t iov_bytes;

    uint64_t flush_off;
    disk_op flush_op;
};

static int
pstate_iov_flush(struct pstate_iov_collector *x)
{
    if (x->iov_bytes > 0) {
	disk_io_status s =
	    stackwrap_disk_iov(x->flush_op, x->iov_buf, x->iov_cnt, x->flush_off);
	if (!SAFE_EQUAL(s, disk_io_success)) {
	    cprintf("pstate_iov_flush: error during disk io\n");
	    return -E_IO;
	}
    }

    x->flush_off += x->iov_bytes;
    x->iov_bytes = 0;
    x->iov_cnt = 0;
    return 0;
}

static int
pstate_iov_append(struct pstate_iov_collector *x, void *buf, uint32_t size)
{
    uint32_t iov_max = sizeof(x->iov_buf) / sizeof(x->iov_buf[0]);

    if (x->iov_cnt == iov_max) {
	int r = pstate_iov_flush(x);
	if (r < 0)
	    return r;
    }

    x->iov_buf[x->iov_cnt].iov_base = buf;
    x->iov_buf[x->iov_cnt].iov_len = size;
    x->iov_cnt++;
    x->iov_bytes += size;
    return 0;
}

//////////////////////////////////
// Swap-in code
//////////////////////////////////

static int
pstate_swapin_mobj(struct mobject mobj)
{
    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("pstate_swapin_obj: cannot alloc page: %s\n", e2s(r));
	return r;
    }

    struct kobject *ko = (struct kobject *) p;
    pagetree_init(&ko->ko_pt);

    struct pstate_iov_collector x;
    memset(&x, 0, sizeof(x));

    x.flush_off = mobj.off;
    x.flush_op = op_read;

    r = pstate_iov_append(&x, p, KOBJ_DISK_SIZE);
    if (r < 0)
	goto err;

    uint64_t ko_bytes = mobj.nbytes - KOBJ_DISK_SIZE;
    for (uint64_t page = 0; page < ROUNDUP(ko_bytes, PGSIZE) / PGSIZE; page++) {
	r = page_alloc(&p);
	if (r < 0) {
	    cprintf("pstate_swapin_obj: cannot alloc page: %s\n", e2s(r));
	    return r;
	}

	r = pagetree_put_page(&ko->ko_pt, page, p);
	if (r < 0) {
	    page_free(p);
	    goto err;
	}

	uint32_t pagebytes = MIN(ROUNDUP(ko_bytes - page * PGSIZE,
					 512), (uint32_t) PGSIZE);
	if (pagebytes != PGSIZE)
	    memset(p, 0, PGSIZE);

	r = pstate_iov_append(&x, p, pagebytes);
	if (r < 0)
	    goto err;
    }

    r = pstate_iov_flush(&x);
    if (r < 0)
	goto err;

    if (pstate_swapin_debug)
	cprintf("pstate_swapin_obj: id %ld nbytes %ld\n",
			ko->hdr.ko_id, ko->hdr.ko_nbytes);

    kobject_swapin(ko);
    return 0;

err:
    pagetree_free(&ko->ko_pt);
    page_free(p);
    return r;
}

static int
pstate_swapin_id(kobject_id_t id)
{
    kobject_id_t id_found;
    struct mobject mobj;

    int r = btree_search(BTREE_OBJMAP, &id, &id_found, (uint64_t *) &mobj);
    if (r == -E_NOT_FOUND) {
	if (pstate_swapin_debug)
	    cprintf("pstate_swapin_stackwrap: id %ld not found\n", id);
	kobject_negative_insert(id);
    } else if (r < 0) {
	cprintf("pstate_swapin_stackwrap: error during lookup: %s\n", e2s(r));
    } else {
	r = pstate_swapin_mobj(mobj);
	if (r < 0)
	    cprintf("pstate_swapin_stackwrap: swapping in: %s\n", e2s(r));
    }

    return r;
}

static void
pstate_swapin_stackwrap(void *arg)
{
    static struct Thread_list swapin_waiting;
    static int swapin_active;

    if (cur_thread)
	thread_suspend(cur_thread, &swapin_waiting);

    // XXX
    // The reason for having only one swapin at a time is to avoid
    // swapping in the same object twice.
    if (swapin_active)
	return;
    swapin_active = 1;

    kobject_id_t id = (kobject_id_t) arg;
    pstate_swapin_id(id);

    swapin_active = 0;

    while (!LIST_EMPTY(&swapin_waiting)) {
	struct Thread *t = LIST_FIRST(&swapin_waiting);
	thread_set_runnable(t);
    }
}

int
pstate_swapin(kobject_id_t id)
{
    if (pstate_swapin_debug)
	cprintf("pstate_swapin: object %ld\n", id);

    int r = stackwrap_call(&pstate_swapin_stackwrap, (void *) id);
    if (r < 0) {
	cprintf("pstate_swapin: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    return -E_RESTART;
}

/////////////////////////////////////
// Persistent-store initialization
/////////////////////////////////////

static int pstate_sync_apply(struct pstate_header *hdr) ;

static int
pstate_load2(void)
{
    disk_io_status s = stackwrap_disk_io(op_read, &pstate_buf.buf[0], PSTATE_BUF_SIZE, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_load2: cannot read header\n");
	return -E_IO;
    }

    memcpy(&stable_hdr, &pstate_buf.hdr, sizeof(stable_hdr));
    if (stable_hdr.ph_magic != PSTATE_MAGIC ||
	stable_hdr.ph_version != PSTATE_VERSION)
    {
	cprintf("pstate_load2: magic/version mismatch\n");
	return -E_INVAL;
    }

    log_init();

    if (stable_hdr.ph_applying) {
	cprintf("pstate_load2: applying log\n");
	pstate_sync_apply(&pstate_buf.hdr);
	memcpy(&stable_hdr, &pstate_buf.hdr, sizeof(stable_hdr));
    }

    freelist_deserialize(&freelist, &stable_hdr.ph_free);
    btree_manager_deserialize(&stable_hdr.ph_btrees) ;

    struct btree_traversal trav;
    int r = btree_init_traversal(BTREE_IOBJ, &trav);
    if (r < 0)
	return r;

    while (btree_next_entry(&trav)) {
	uint64_t id = *trav.key;
	if (pstate_load_debug)
	    cprintf("pstate_load2: paging in kobj %ld\n", id);

	r = pstate_swapin_id(id);
	if (r < 0) {
	    cprintf("pstate_load2: cannot swapin %ld: %s\n", id, e2s(r));
	    return r;
	}
    }

    handle_counter   = stable_hdr.ph_handle_counter;
    user_root_handle = stable_hdr.ph_user_root_handle;
    memcpy(&handle_key[0], &stable_hdr.ph_handle_key, HANDLE_KEY_SIZE);

    if (timer_user_msec < stable_hdr.ph_user_msec)
	timer_user_msec_offset = stable_hdr.ph_user_msec - timer_user_msec;

    if (pstate_load_debug)
	cprintf("pstate_load2: handle_ctr %ld root_handle %ld msec %ld\n",
		handle_counter, user_root_handle, timer_user_msec);

    return 1;
}

static void
pstate_load_stackwrap(void *arg)
{
    int *donep = (int *) arg;
    *donep = pstate_load2();
}

static void
pstate_reset(void)
{
    memset(&stable_hdr, 0, sizeof(stable_hdr));
}

int
pstate_load(void)
{
    int done = 0;
    int r = stackwrap_call(&pstate_load_stackwrap, &done);
    if (r < 0) {
	cprintf("pstate_load: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    uint64_t ts_start = read_tsc();
    int warned = 0;
    while (!done) {
	uint64_t ts_now = read_tsc();
	if (warned == 0 && ts_now - ts_start > 1024*1024*1024) {
	    cprintf("pstate_load: wedged for %ld\n", ts_now - ts_start);
	    warned = 1;
	}
	ide_intr();
    }

    if (done < 0)
	pstate_reset();

    return done;
}

//////////////////////////////////////////////////
// Swap-out code
//////////////////////////////////////////////////

struct swapout_stats {
    uint64_t written_kobj;
    uint64_t written_pages;
    uint64_t snapshoted_kobj;
    uint64_t dead_kobj;
    uint64_t total_kobj;
};

static int
pstate_sync_kobj(struct swapout_stats *stats,
		 struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    int64_t off = pstate_kobj_alloc(&freelist, snap);
    if (off < 0) {
	cprintf("pstate_sync_kobj: cannot allocate space: %s\n", e2s(off));
	return off;
    }

    struct pstate_iov_collector x;
    memset(&x, 0, sizeof(x));

    x.flush_off = off;
    x.flush_op = op_write;

    int r = pstate_iov_append(&x, snap, KOBJ_DISK_SIZE);
    if (r < 0)
	return r;

    for (uint64_t page = 0; page < kobject_npages(&snap->hdr); page++) {
	void *p;
	r = kobject_get_page(&snap->hdr, page, &p, page_ro);
	if (r < 0)
	    panic("pstate_sync_kobj: cannot get page: %s", e2s(r));

	uint32_t pagebytes = MIN(ROUNDUP(snap->hdr.ko_nbytes - page * PGSIZE,
					 512), (uint32_t) PGSIZE);
	r = pstate_iov_append(&x, p, pagebytes);
	if (r < 0)
	    return r;

	stats->written_pages++;
    }

    r = pstate_iov_flush(&x);
    if (r < 0)
	return r;

    if (pstate_swapout_debug)
	cprintf("pstate_sync_kobj: id %ld nbytes %ld\n",
		snap->hdr.ko_id, snap->hdr.ko_nbytes);

    kobject_snapshot_release(ko);
    stats->written_kobj++;
    return 0;
}

static int
pstate_sync_apply(struct pstate_header *hdr)
{
    // 1st, mark that applying
    hdr->ph_applying = 1 ;
    disk_io_status s = stackwrap_disk_io(op_write, hdr,PSTATE_BUF_SIZE, 
                                         HEADER_OFFSET * PGSIZE);
    if (!SAFE_EQUAL(s, disk_io_success)) {
    	cprintf("pstate_sync_apply: unable to mark applying\n");
    	return -E_IO ;	
    }

    // 2nd, read flushed header copy
    s = stackwrap_disk_io(op_read, hdr, PSTATE_BUF_SIZE, 
                          HEADER_OFFSET2 * PGSIZE);
    if (!SAFE_EQUAL(s, disk_io_success)) {
    	cprintf("pstate_sync_apply: unable to read header\n");
    	return -E_IO;
    }
    
    // 3rd, apply node log
    int r = log_apply();
    if (r < 0)
	   return r;

    // 4th, write out new header
    s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 
                          HEADER_OFFSET * PGSIZE);
    if (!SAFE_EQUAL(s, disk_io_success)) {
    	cprintf("pstate_sync_apply: unable to write header\n");
    	return -E_IO;
    }

    // 5th, unmark applying
    hdr->ph_applying = 0;
    s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 
                          HEADER_OFFSET * PGSIZE);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	   cprintf("pstate_sync_apply: unable to unmark applying\n");
	   return -E_IO;
    }

    return 0;
}

static int
pstate_sync_flush(struct pstate_header *hdr)
{
    // 1st, write a copy of the header
    disk_io_status s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 
                                         HEADER_OFFSET2 * PGSIZE);
    if (!SAFE_EQUAL(s, disk_io_success)) {
    	cprintf("pstate_sync_flush: unable to flush hdr\n");
    	return -E_IO;
    }

    // 2nd, flush the node log
    int r = log_flush();
    if (r < 0)
	   return r;

    return 0;
}

static int
pstate_sync_loop(struct pstate_header *hdr,
		 struct swapout_stats *stats)
{
    struct kobject *ko;
    LIST_FOREACH(ko, &ko_list, ko_link) {
        if (!(ko->hdr.ko_flags & KOBJ_SNAPSHOTING))
            continue;
        
        struct kobject *snap = kobject_get_snapshot(&ko->hdr);
        if (snap->hdr.ko_type == kobj_dead) {
            pstate_kobj_free(&freelist, snap);
            stats->dead_kobj++;
            continue;
        }
        
        int r = pstate_sync_kobj(stats, &ko->hdr);
        if (r < 0)
            return r;
    }

    int r = freelist_commit(&freelist);
    if (r < 0) {
        cprintf("pstate_sync_loop: cannot commit freelist: %s\n", e2s(r));
        return r;
    }

    btree_lock_all() ;
    
    freelist_serialize(&hdr->ph_free, &freelist);
    btree_manager_serialize(&hdr->ph_btrees);

    r = pstate_sync_flush(hdr);
    if (r < 0) {
    	cprintf("pstate_sync_loop: unable to flush\n") ;
    	return r ;
    }

    r = pstate_sync_apply(hdr);
    if (r < 0) {
    	cprintf("pstate_sync_loop: unable to apply\n");
    	return r;
    }

    btree_unlock_all() ;

    memcpy(&stable_hdr, hdr, sizeof(stable_hdr));
    return 0;
}

static void
pstate_sync_stackwrap(void *arg __attribute__((unused)))
{
    static int swapout_active;

    if (swapout_active) {
    	cprintf("pstate_sync: another sync still active\n");
    	return;
    }
    swapout_active = 1;

    // If we don't have a valid header on disk, init the freelist
    if (stable_hdr.ph_magic != PSTATE_MAGIC) {
    	uint64_t disk_pages = disk_bytes / PGSIZE ;
    	uint64_t reserved_pages = RESERVED_PAGES ;
    	assert(disk_pages > reserved_pages);
    
    	if (pstate_swapout_debug)
    	    cprintf("pstate_sync: %ld disk pages\n", disk_pages);
    
        log_init(); 
        btree_manager_init();
    	freelist_init(&freelist, reserved_pages * PGSIZE,
		      (disk_pages - reserved_pages) * PGSIZE);
    }

    static_assert(sizeof(pstate_buf.hdr) <= PSTATE_BUF_SIZE);

    struct pstate_header *hdr = &pstate_buf.hdr;
    memcpy(hdr, &stable_hdr, sizeof(stable_hdr));
    
    hdr->ph_magic = PSTATE_MAGIC;
    hdr->ph_version = PSTATE_VERSION;
    hdr->ph_handle_counter = handle_counter;
    hdr->ph_user_root_handle = user_root_handle;
    hdr->ph_user_msec = timer_user_msec;
    memcpy(&hdr->ph_handle_key[0], &handle_key[0], HANDLE_KEY_SIZE);

    struct swapout_stats stats;
    memset(&stats, 0, sizeof(stats));

    struct kobject *ko, *ko_next;
    LIST_FOREACH(ko, &ko_list, ko_link) {
    	stats.total_kobj++;
    	if ((ko->hdr.ko_flags & KOBJ_DIRTY)) {
    	    kobject_snapshot(&ko->hdr);
    	    ko->hdr.ko_flags |= KOBJ_SNAPSHOT_DIRTY;
    	    stats.snapshoted_kobj++;
    	}
    }

    int r = pstate_sync_loop(hdr, &stats);
    if (r < 0)
	   cprintf("pstate_sync_stackwrap: cannot sync: %s\n", e2s(r));

    for (ko = LIST_FIRST(&ko_list); ko; ko = ko_next) {
    	ko_next = LIST_NEXT(ko, ko_link);
    
    	if ((ko->hdr.ko_flags & KOBJ_SNAPSHOT_DIRTY)) {
    	    ko->hdr.ko_flags &= ~KOBJ_SNAPSHOT_DIRTY;
    	    if (r < 0)
    		ko->hdr.ko_flags |= KOBJ_DIRTY;
    	}
    
    	if ((ko->hdr.ko_flags & KOBJ_SNAPSHOTING)) {
    	    struct kobject *snap = kobject_get_snapshot(&ko->hdr);
    	    kobject_snapshot_release(&ko->hdr);
    
    	    if (r == 0 && snap->hdr.ko_type == kobj_dead)
    		kobject_swapout(ko);
    	}
    }

    if (pstate_swapout_stats) {
    	cprintf("pstate_sync: total %ld snap %ld dead %ld wrote %ld pages %ld\n",
    		stats.total_kobj, stats.snapshoted_kobj, stats.dead_kobj,
    		stats.written_kobj, stats.written_pages);
    	cprintf("pstate_sync: pages used %ld avail %ld allocs %ld fail %ld\n",
    		page_stats.pages_used, page_stats.pages_avail,
    		page_stats.allocations, page_stats.failures);
    }

    swapout_active = 0;
}


static void
pstate_sync(void)
{
    int r = stackwrap_call(&pstate_sync_stackwrap, 0);
    if (r < 0)
	   cprintf("pstate_sync: cannot stackwrap: %s\n", e2s(r));
}

void
pstate_init(void)
{
    pstate_reset();

    static struct periodic_task sync_pt = { .pt_fn = &pstate_sync };
    sync_pt.pt_interval_ticks = kclock_hz * 3600;
    timer_add_periodic(&sync_pt);
}
