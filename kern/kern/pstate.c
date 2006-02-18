#include <machine/pmap.h>
#include <machine/thread.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/log.h>
#include <inc/error.h>
#include <lib/btree/btree_traverse.h>
#include <lib/btree/btree_debug.h>

// verbose flags
static int pstate_load_debug = 0;
static int pstate_swapin_debug = 0;
static int pstate_swapout_debug = 0;
static int pstate_swapout_stats = 0;
static int pstate_dlog_stats = 0;

static int scrub_disk_pages = 0;

// Authoritative copy of the header that's actually on disk.
static struct pstate_header stable_hdr;

// assumed to be atomic
static struct freelist flist ;

// lits of objects to be loaded at startup
struct btree_default iobjlist ;
#define IOBJ_ORDER BTREE_MAX_ORDER1
STRUCT_BTREE_CACHE(iobj_cache, 20, IOBJ_ORDER, 1) ;	

// lits of objects to be loaded at startup
struct btree_default objmap ;
#define OBJMAP_ORDER BTREE_MAX_ORDER1
STRUCT_BTREE_CACHE(objmap_cache, 20, OBJMAP_ORDER, 1) ;	

struct mobject
{
	offset_t off ;
	uint64_t npages ;	
	
} ;

// Scratch-space for a copy of the header used while reading/writing.
#define N_HEADER_PAGES		1
#define PSTATE_BUF_SIZE		(N_HEADER_PAGES * PGSIZE)
static union {
    struct pstate_header hdr;
    char buf[PSTATE_BUF_SIZE];
} pstate_buf;

// all units are in pages
#define LOG_OFFSET	N_HEADER_PAGES
#define LOG_SIZE	200
#define LOG_MEMORY	100


//////////////////////////////////////////////////
// Object map
//////////////////////////////////////////////////

static void
pstate_kobj_free(struct freelist *f, struct kobject *ko)
{
    
	uint64_t key ;
	struct mobject mobj ;

    int r = btree_search(&objmap.tree, &ko->hdr.ko_id, &key, (uint64_t *)&mobj) ;
    if (r == 0) {
    	assert(key == ko->hdr.ko_id) ;

	if (scrub_disk_pages) {
	    void *p;
	    assert(0 == page_alloc(&p));
	    memset(p, 0xc4, PGSIZE);

	    for (uint32_t i = 0; i < mobj.npages; i++)
		stackwrap_disk_io(op_write, p, PGSIZE, mobj.off * PGSIZE);

	    page_free(p);
	}

	freelist_free_later(f, mobj.off, mobj.npages) ;
    	btree_delete(&iobjlist.tree, &ko->hdr.ko_id) ;
    	btree_delete(&objmap.tree, &ko->hdr.ko_id) ;
    }
}

static int64_t
pstate_kobj_alloc(struct freelist *f, struct kobject *ko)
{
    int r ;
    pstate_kobj_free(f, ko);
	

	uint64_t npages = ko->hdr.ko_npages + 1;
    int64_t offset = freelist_alloc(f, npages);
	
	if (offset < 0) {
		cprintf("pstate_kobj_alloc: no room for %ld pages\n", npages);
		return offset;
    }

	struct mobject mobj = { offset, npages } ;
	r = btree_insert(&objmap.tree, &ko->hdr.ko_id, (uint64_t *)&mobj) ;
	if (r < 0) {
		cprintf("pstate_kobj_alloc: objmap insert failed, disk full?\n") ;
		return r ;
	}

	if (kobject_initial(ko)) {
		r = btree_insert(&iobjlist.tree, &ko->hdr.ko_id, &offset) ;
		if (r < 0) {
			cprintf("pstate_kobj_alloc: iobjlist insert failed, "
				"disk full?\n") ;
			return r ;	
		}
	}
	return offset ;
}

//////////////////////////////////
// Swap-in code
//////////////////////////////////

static int
pstate_swapin_off(offset_t off)
{
    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
		cprintf("pstate_swapin_obj: cannot alloc page: %s\n", e2s(r));
		return r;
    }

    struct kobject *ko = (struct kobject *) p;
    offset_t offset = PGSIZE * off ;
    
    disk_io_status s = stackwrap_disk_io(op_read, p, PGSIZE, offset);
    if (s != disk_io_success) {
		cprintf("pstate_swapin_obj: cannot read object from disk\n");
		return -E_IO;
    }

    pagetree_init(&ko->hdr.ko_pt);
    for (uint64_t page = 0; page < ko->hdr.ko_npages; page++) {
		r = page_alloc(&p);
		if (r < 0) {
		    cprintf("pstate_swapin_obj: cannot alloc page: %s\n", e2s(r));
		    return r;
		}
	
		offset = (off + page + 1) * PGSIZE;
		s = stackwrap_disk_io(op_read, p, PGSIZE, offset);
		if (s != disk_io_success) {
		    cprintf("pstate_swapin_obj: cannot read page from disk\n");
		    return -E_IO;
		}
	
		assert(0 == pagetree_put_page(&ko->hdr.ko_pt, page, p));
    }

    if (pstate_swapin_debug)
	cprintf("pstate_swapin_obj: id %ld npages %ld\n",
			ko->hdr.ko_id, ko->hdr.ko_npages);

    kobject_swapin(ko);
    return 0;
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
    kobject_id_t id_found;
    struct mobject mobj;
    int r = btree_search(&objmap.tree, &id, &id_found, (uint64_t *) &mobj);
    if (r == -E_NOT_FOUND) {
	if (pstate_swapin_debug)
	    cprintf("pstate_swapin_stackwrap: id %ld not found\n", id);
	kobject_negative_insert(id);
    } else if (r < 0) {
	cprintf("pstate_swapin_stackwrap: error during lookup: %s\n", e2s(r));
    } else {
	r = pstate_swapin_off(mobj.off);
	if (r < 0)
	    cprintf("pstate_swapin_stackwrap: swapping in: %s\n", e2s(r));
    }

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

static int pstate_sync_apply(void) ;

static int
pstate_load2(void)
{
    disk_io_status s = stackwrap_disk_io(op_read, &pstate_buf.buf[0], PSTATE_BUF_SIZE, 0);
    if (s != disk_io_success) {
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

	dlog_init() ;
	log_init(LOG_OFFSET + 1, LOG_SIZE - 1, LOG_MEMORY) ;
	
	if(stable_hdr.ph_applying) {
		cprintf("pstate_load2: applying log\n") ;
		pstate_sync_apply() ;
	    memcpy(&stable_hdr, &pstate_buf.hdr, sizeof(stable_hdr));
	}
	
	// XXX
	memcpy(&flist, &stable_hdr.ph_free, sizeof(flist)) ;
	freelist_setup((uint8_t *)&flist) ;
	memcpy(&iobjlist, &stable_hdr.ph_iobjs, sizeof(iobjlist)) ;
	btree_default_setup(&iobjlist, IOBJ_ORDER, &flist, &iobj_cache) ;
	memcpy(&objmap, &stable_hdr.ph_map, sizeof(objmap)) ;
	btree_default_setup(&objmap, OBJMAP_ORDER, &flist, &iobj_cache) ;

	struct btree_traversal trav ;
	btree_init_traversal(&iobjlist.tree, &trav) ;
	
	if (pstate_load_debug)
		btree_pretty_print(&iobjlist.tree, iobjlist.tree.root, 0);
	
	while (btree_next_entry(&trav)) {
		
		uint64_t id = *trav.key ;
		offset_t off = *trav.val ;
		if (pstate_load_debug)
			cprintf("pstate_load2: paging in kobj %ld\n", id) ;
		
		int r = pstate_swapin_off(off) ;
		
		if (r < 0) {
			cprintf("pstate_load2: cannot swapin offset %ld: %s\n",
					id, e2s(r)) ;
			return r ;
		}
	}
		
    handle_counter   = stable_hdr.ph_handle_counter;
    user_root_handle = stable_hdr.ph_user_root_handle;

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

void
pstate_init(void)
{
    pstate_reset();

    static struct periodic_task sync_pt = { .pt_fn = &pstate_sync };
    sync_pt.pt_interval_ticks = kclock_hz;
    timer_add_periodic(&sync_pt);
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

struct pstate_iov_collector {
    struct iovec *iov_buf;
    int iov_cnt;
    int iov_max;

    uint32_t iov_bytes;
    uint64_t flush_off;
};

static int
pstate_iov_flush(struct pstate_iov_collector *x)
{
    if (x->iov_bytes > 0) {
	disk_io_status s =
	    stackwrap_disk_iov(op_write, x->iov_buf, x->iov_cnt, x->flush_off);
	if (s != disk_io_success) {
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
    if (x->iov_cnt == x->iov_max) {
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

static int
pstate_sync_kobj(struct swapout_stats *stats,
		 struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    int64_t off = pstate_kobj_alloc(&flist, snap);
    if (off < 0) {
	cprintf("pstate_sync_kobj: cannot allocate space: %s\n", e2s(off));
	return off;
    }

    struct pstate_iov_collector x;
    memset(&x, 0, sizeof(x));

    struct iovec iov_buf[DISK_REQMAX / PGSIZE];
    x.flush_off = off * PGSIZE;
    x.iov_buf = &iov_buf[0];
    x.iov_max = sizeof(iov_buf) / sizeof(iov_buf[0]);

    int r = pstate_iov_append(&x, snap, PGSIZE);
    if (r < 0)
	return r;

    for (uint64_t page = 0; page < snap->hdr.ko_npages; page++) {
	void *p;
	r = kobject_get_page(&snap->hdr, page, &p, page_ro);
	if (r < 0)
	    panic("pstate_sync_kobj: cannot get page: %s", e2s(r));

	r = pstate_iov_append(&x, p, PGSIZE);
	if (r < 0)
	    return r;

	stats->written_pages++;
    }

    r = pstate_iov_flush(&x);
    if (r < 0)
	return r;

    if (pstate_swapout_debug)
	cprintf("pstate_sync_kobj: id %ld npages %ld\n",
		snap->hdr.ko_id, snap->hdr.ko_npages);

    kobject_snapshot_release(ko);
    stats->written_kobj++;
    return 0;
}

static int
pstate_sync_apply(void)
{
	struct pstate_header *hdr = &pstate_buf.hdr ;


	// 1st, mark that appplying
    hdr->ph_applying = 1 ;
    disk_io_status s = stackwrap_disk_io(op_write, hdr, 
    									 PSTATE_BUF_SIZE, 0) ;
    if (s != disk_io_success) {
    	cprintf("pstate_sync_apply: unable to mark applying\n") ;
    	return -E_IO ;	
    }

    // 2nd, read flushed header copy
    s = stackwrap_disk_io(op_read, hdr, 
 						 PSTATE_BUF_SIZE, LOG_OFFSET * PGSIZE) ;
    if (s != disk_io_success) {
    	cprintf("pstate_sync_apply: unable to read header\n") ;
    	return -E_IO ;	
    }
    
	// 3rd, apply node log
	int r = log_apply() ;
	if (r < 0)
		return r ;
		
	// 4th, write out new header
	s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 0) ;
    if (s != disk_io_success) {
    	cprintf("pstate_sync_apply: unable to write header\n") ;
    	return -E_IO ;	
    }

	// 5th, unmark applying
	hdr->ph_applying = 0 ;
	s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 0) ;
    if (s != disk_io_success) {
    	cprintf("pstate_sync_apply: unable to unmark applying\n") ;
    	return -E_IO ;	
    }

	return 0 ;
}

static int
pstate_sync_flush(void)
{
	struct pstate_header *hdr = &pstate_buf.hdr ;


	// 1st, write a copy of the header
    disk_io_status s = stackwrap_disk_io(op_write, hdr, 
    									 PSTATE_BUF_SIZE, LOG_OFFSET * PGSIZE) ;
    if (s != disk_io_success) {
    	cprintf("pstate_sync_flush: unable to flush hdr\n") ;
    	return -E_IO ;	
    }

    // 2nd, flush the node log
	int r = log_flush() ;
	if (r < 0)
		return r ;
    
    return 0 ;
}

static int
pstate_sync_loop(struct pstate_header *hdr,
		 struct swapout_stats *stats)
{
    struct kobject_hdr *ko;
    LIST_FOREACH(ko, &ko_list, ko_link) {
		if (!(ko->ko_flags & KOBJ_SNAPSHOTING))
		    continue;
	
		struct kobject *snap = kobject_get_snapshot(ko);
		if (snap->hdr.ko_type == kobj_dead) {
		    pstate_kobj_free(&flist, snap);
		    stats->dead_kobj++;
		    continue;
		}

		int r = pstate_sync_kobj(stats, ko);
		if (r < 0)
		    return r;
    }
	
	freelist_commit(&flist) ;
	
	// XXX
	memcpy(&hdr->ph_free, &flist, sizeof(flist)) ;
	memcpy(&hdr->ph_iobjs, &iobjlist, sizeof(iobjlist)) ;
	memcpy(&hdr->ph_map, &objmap, sizeof(objmap)) ;
	
	int r = pstate_sync_flush() ;
	
	if (r < 0) {
		cprintf("pstate_sync_loop: unable to flush\n") ;
		return r ;
	}
		
	r = pstate_sync_apply() ;
	if (r < 0) {
		cprintf("pstate_sync_loop: unable to apply\n") ;
		return r ;
	}

	if (pstate_dlog_stats)
		dlog_print() ;

	memcpy(&stable_hdr, hdr, sizeof(stable_hdr));
	return 0 ;
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
		uint64_t disk_pages = disk_bytes / PGSIZE;
		assert(disk_pages > N_HEADER_PAGES);
	
		if (pstate_swapout_debug)
		    cprintf("pstate_sync: %ld disk pages\n", disk_pages);
	
		dlog_init() ;
		log_init(LOG_OFFSET + 1, LOG_SIZE - 1, LOG_MEMORY) ;
		
		freelist_init(&flist,
			      N_HEADER_PAGES + LOG_SIZE,
			      disk_pages - N_HEADER_PAGES - LOG_SIZE);
		btree_default_init(&iobjlist, IOBJ_ORDER, 1, 1, &flist, &iobj_cache) ;
		btree_default_init(&objmap, OBJMAP_ORDER, 1, 2, &flist, &objmap_cache) ;
    }

    static_assert(sizeof(pstate_buf.hdr) <= PSTATE_BUF_SIZE);
    static_assert(BTREE_NODE_SIZE(IOBJ_ORDER, 1) <= PGSIZE) ;
	static_assert(BTREE_NODE_SIZE(OBJMAP_ORDER, 1) <= PGSIZE) ;
	
    struct pstate_header *hdr = &pstate_buf.hdr;
    memcpy(hdr, &stable_hdr, sizeof(stable_hdr));
    
    hdr->ph_magic = PSTATE_MAGIC;
    hdr->ph_version = PSTATE_VERSION;
    hdr->ph_handle_counter = handle_counter;
    hdr->ph_user_root_handle = user_root_handle;
    hdr->ph_user_msec = timer_user_msec;

    struct swapout_stats stats;
    memset(&stats, 0, sizeof(stats));

    struct kobject_hdr *ko, *ko_next;
    LIST_FOREACH(ko, &ko_list, ko_link) {
		stats.total_kobj++;
		if ((ko->ko_flags & KOBJ_DIRTY)) {
		    kobject_snapshot(ko);
		    ko->ko_flags |= KOBJ_SNAPSHOT_DIRTY;
		    stats.snapshoted_kobj++;
		}
    }

    int r = pstate_sync_loop(hdr, &stats);
    if (r < 0)
		cprintf("pstate_sync_stackwrap: cannot sync: %s\n", e2s(r));

    for (ko = LIST_FIRST(&ko_list); ko; ko = ko_next) {
		ko_next = LIST_NEXT(ko, ko_link);
	
		if ((ko->ko_flags & KOBJ_SNAPSHOT_DIRTY)) {
		    ko->ko_flags &= ~KOBJ_SNAPSHOT_DIRTY;
		    if (r < 0)
				ko->ko_flags |= KOBJ_DIRTY;
		}
	
		if ((ko->ko_flags & KOBJ_SNAPSHOTING)) {
		    struct kobject *snap = kobject_get_snapshot(ko);
		    kobject_snapshot_release(ko);
	
		    if (r == 0 && snap->hdr.ko_type == kobj_dead)
				kobject_swapout(kobject_h2k(ko));
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


void
pstate_sync(void)
{
    int r = stackwrap_call(&pstate_sync_stackwrap, 0);
    if (r < 0)
	cprintf("pstate_sync: cannot stackwrap: %s\n", e2s(r));
}
