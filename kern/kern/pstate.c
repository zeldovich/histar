#include <machine/x86.h>
#include <dev/disk.h>
#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/disklayout.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/log.h>
#include <kern/stackwrap.h>
#include <inc/error.h>

// verbose flags
static int pstate_load_debug = 0;
static int pstate_swapin_debug = 0;
static int pstate_swapout_debug = 0;
static int pstate_swapout_object_debug = 0;
static int pstate_swapout_stats = 0;

static int scrub_disk_pages = 0;
static int commit_panic = 0;

// Authoritative copy of the header that's actually on disk.
static struct pstate_header stable_hdr;

// Global freelist for disk
struct freelist freelist;

struct mobject {
    offset_t off;
    uint64_t nbytes;
};

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
	cprintf("pstate_kobj_alloc: no room for %"PRIu64" bytes\n", nbytes);
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
    struct kiovec iov_buf[DISK_REQMAX / PGSIZE];
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
pstate_swapin_mobj(struct mobject mobj, kobject_id_t id)
{
    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("pstate_swapin_obj: cannot alloc page: %s\n", e2s(r));
	return r;
    }

    struct kobject *ko = (struct kobject *) p;
    memset(ko, 0, sizeof(*ko));
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
	    goto err;
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
	cprintf("pstate_swapin_obj: id %"PRIu64" nbytes %"PRIu64"\n",
			ko->hdr.ko_id, ko->hdr.ko_nbytes);

    if (ko->hdr.ko_id != id) {
	cprintf("pstate_swapin_mobj: requested %"PRIu64" (%"PRIu64" @ %"PRIu64"), got %"PRIu64"\n",
		id, mobj.nbytes, mobj.off, ko->hdr.ko_id);

	kobject_id_t id_found;
	r = btree_search(BTREE_OBJMAP, &ko->hdr.ko_id, &id_found, (uint64_t *) &mobj);
	if (r >= 0)
	    cprintf("pstate_swapin_mobj: %"PRIu64" maps to %"PRIu64" @ %"PRIu64"\n",
		    id_found, mobj.nbytes, mobj.off);

	panic("pstate_swapin_mobj: disk state corrupted");
    }

    kobject_swapin(ko);
    return 0;

err:
    pagetree_free(&ko->ko_pt);
    page_free(ko);
    return r;
}

static int
pstate_swapin_id(kobject_id_t id)
{
    kobject_id_t id_found;
    struct mobject mobj;

    if (stable_hdr.ph_magic != PSTATE_MAGIC) {
	cprintf("pstate_swapin_id: disk not initialized\n");
	kobject_negative_insert(id);
	return -E_NOT_FOUND;
    }

    int r = btree_search(BTREE_OBJMAP, &id, &id_found, (uint64_t *) &mobj);
    if (r == -E_NOT_FOUND) {
	if (pstate_swapin_debug)
	    cprintf("pstate_swapin_id: id %"PRIu64" not found\n", id);
	kobject_negative_insert(id);
    } else if (r < 0) {
	cprintf("pstate_swapin_stackwrap: error during lookup: %s\n", e2s(r));
    } else {
	r = pstate_swapin_mobj(mobj, id);
	if (r < 0)
	    cprintf("pstate_swapin_id: swapping in: %s\n", e2s(r));
    }

    return r;
}

static void
pstate_swapin_stackwrap(uint64_t arg, uint64_t arg1 __attribute__((unused)), uint64_t arg2 __attribute__((unused)))
{
    kobject_id_t id = (kobject_id_t) arg;
    static struct Thread_list swapin_waiting;
    static struct lock swapin_lock;

    if (cur_thread)
	thread_suspend(cur_thread, &swapin_waiting);

    // XXX
    // The reason for having only one swapin at a time is to avoid
    // swapping in the same object twice.
    if (lock_try_acquire(&swapin_lock) < 0)
	return;

    pstate_swapin_id(id);

    lock_release(&swapin_lock);
    while (!LIST_EMPTY(&swapin_waiting))
	thread_set_runnable(LIST_FIRST(&swapin_waiting));
}

int
pstate_swapin(kobject_id_t id)
{
    if (pstate_swapin_debug)
	cprintf("pstate_swapin: object %"PRIu64"\n", id);

    int r = stackwrap_call(&pstate_swapin_stackwrap, id, 0, 0);
    if (r < 0) {
	cprintf("pstate_swapin: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    // If the thread is still runnable, don't claim -E_RESTART.
    if (cur_thread && SAFE_EQUAL(cur_thread->th_status, thread_runnable))
	return 0;
    return -E_RESTART;
}

/////////////////////////////////////
// Persistent-store initialization
/////////////////////////////////////

static int
pstate_apply_disk_log(void)
{
    log_init();

    int r = log_apply_disk(stable_hdr.ph_log_blocks);
    if (r < 0) {
	cprintf("pstate_apply_disk_log: cannot apply: %s\n", e2s(r));
	return r;
    }

    disk_io_status s = stackwrap_disk_io(op_flush, 0, 0, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_apply_disk_log: cannot flush\n");
	return -E_IO;
    }

    stable_hdr.ph_log_blocks = 0;
    s = stackwrap_disk_io(op_write, &stable_hdr.ph_buf[0],
			  PSTATE_BUF_SIZE, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_apply_disk_log: cannot write out header\n");
	return -E_IO;
    }

    s = stackwrap_disk_io(op_flush, 0, 0, 0);
    if (!SAFE_EQUAL(s, disk_io_success))
	panic("pstate_apply_disk_log: cannot flush header");

    return 0;
}

static int
pstate_load2(void)
{
    disk_io_status s = stackwrap_disk_io(op_read, &stable_hdr.ph_buf[0],
					 PSTATE_BUF_SIZE, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_load2: cannot read header\n");
	return -E_IO;
    }

    if (stable_hdr.ph_magic != PSTATE_MAGIC ||
	stable_hdr.ph_version != PSTATE_VERSION)
    {
	cprintf("pstate_load2: magic/version mismatch\n");
	return -E_INVAL;
    }

    int r = pstate_apply_disk_log();
    if (r < 0) {
	cprintf("pstate_load2: cannot apply log: %s\n", e2s(r));
	return r;
    }

    freelist_deserialize(&freelist, &stable_hdr.ph_free);
    btree_manager_deserialize(&stable_hdr.ph_btrees);

    struct btree_traversal trav;
    r = btree_init_traversal(BTREE_IOBJ, &trav);
    if (r < 0)
	return r;

    while (btree_next_entry(&trav)) {
	uint64_t id = *trav.key;
	if (pstate_load_debug)
	    cprintf("pstate_load2: paging in kobj %"PRIu64"\n", id);

	r = pstate_swapin_id(id);
	if (r < 0) {
	    cprintf("pstate_load2: cannot swapin %"PRIu64": %s\n", id, e2s(r));
	    return r;
	}
    }

    handle_counter   = stable_hdr.ph_handle_counter;
    user_root_handle = stable_hdr.ph_user_root_handle;
    memcpy(&handle_key[0], &stable_hdr.ph_handle_key, HANDLE_KEY_SIZE);

    if (timer_user_msec < stable_hdr.ph_user_msec)
	timer_user_msec_offset = stable_hdr.ph_user_msec - timer_user_msec;

    if (pstate_load_debug)
	cprintf("pstate_load2: handle_ctr %"PRIu64" root_handle %"PRIu64" msec %"PRIu64"\n",
		handle_counter, user_root_handle, timer_user_msec);

    return 1;
}

static void
pstate_load_stackwrap(uint64_t arg, uint64_t arg1 __attribute__((unused)), uint64_t arg2 __attribute__((unused)))
{
    int *donep = (int *) (uintptr_t) arg;
    *donep = pstate_load2();
}

static void
pstate_reset(void)
{
    memset(&stable_hdr, 0, sizeof(stable_hdr));
    log_init();
}

int
pstate_load(void)
{
    int done = 0;
    int r = stackwrap_call(&pstate_load_stackwrap, (uintptr_t) &done, 0, 0);
    if (r < 0) {
	cprintf("pstate_load: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    uint64_t ts_start = read_tsc();
    int warned = 0;
    while (!done) {
	uint64_t ts_now = read_tsc();
	if (warned == 0 && ts_now - ts_start > 1024*1024*1024) {
	    cprintf("pstate_load: wedged for %"PRIu64"\n", ts_now - ts_start);
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

static struct Thread_list swapout_waiting;
static struct lock swapout_lock;

struct swapout_stats {
    uint64_t written_kobj;
    uint64_t written_pages;
    uint64_t snapshoted_kobj;
    uint64_t dead_kobj;
    uint64_t total_kobj;
};

static int
pstate_sync_kobj(struct pstate_header *hdr,
		 struct swapout_stats *stats,
		 struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);
    snap->hdr.ko_sync_ts = hdr->ph_sync_ts;

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
	r = kobject_get_page(&snap->hdr, page, &p, page_shared_ro);
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
	cprintf("pstate_sync_kobj: id %"PRIu64" nbytes %"PRIu64"\n",
		snap->hdr.ko_id, snap->hdr.ko_nbytes);

    ko->ko_flags |= KOBJ_ON_DISK;
    kobject_snapshot_release(ko);
    stats->written_kobj++;
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
	if (snap->hdr.ko_type == kobj_dead && (snap->hdr.ko_flags & KOBJ_ON_DISK)) {
	    pstate_kobj_free(&freelist, snap);
	    stats->dead_kobj++;
	    continue;
	}

	int r = pstate_sync_kobj(hdr, stats, &ko->hdr);
	if (r < 0)
	    return r;
    }

    int r = freelist_commit(&freelist);
    if (r < 0) {
	cprintf("pstate_sync_loop: cannot commit freelist: %s\n", e2s(r));
	return r;
    }

    btree_lock_all();

    freelist_serialize(&hdr->ph_free, &freelist);
    btree_manager_serialize(&hdr->ph_btrees);

    int64_t flush_blocks = log_flush();
    if (flush_blocks < 0) {
	cprintf("pstate_sync_loop: unable to flush: %s\n", e2s(flush_blocks));
	btree_unlock_all();
	return flush_blocks;
    }

    disk_io_status s = stackwrap_disk_io(op_flush, 0, 0, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_sync_loop: unable to flush disk\n");
	btree_unlock_all();
	return -E_IO;
    }

    hdr->ph_log_blocks = flush_blocks;
    s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 0);
    if (!SAFE_EQUAL(s, disk_io_success)) {
	cprintf("pstate_sync_loop: unable to commit header\n");
	btree_unlock_all();
	return -E_IO;
    }

    s = stackwrap_disk_io(op_flush, 0, 0, 0);
    if (!SAFE_EQUAL(s, disk_io_success))
	panic("pstate_sync_loop: unable to flush commit record");

    static int commit_count = 0;
    if (commit_panic && ++commit_count == commit_panic)
	panic("commit test");

    if (hdr->ph_log_blocks > LOG_PAGES / 2 || log_must_apply()) {
	if (pstate_swapout_debug)
	    cprintf("pstate_sync_loop: applying on-disk log\n");

	do {
	    r = log_apply_mem();
	    if (r < 0)
		cprintf("pstate_sync_loop: unable to apply, retry: %s\n", e2s(r));
	} while (r < 0);

	hdr->ph_log_blocks = 0;
	do {
	    s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 0);
	    if (!SAFE_EQUAL(s, disk_io_success))
		cprintf("pstate_sync_loop: unable to rewrite header, retrying\n");
	} while (!SAFE_EQUAL(s, disk_io_success));

	s = stackwrap_disk_io(op_flush, 0, 0, 0);
	if (!SAFE_EQUAL(s, disk_io_success))
	    panic("pstate_sync_loop: unable to flush applied log");
    }

    btree_unlock_all();
    memcpy(&stable_hdr, hdr, sizeof(stable_hdr));
    return 0;
}

static void
pstate_sync_stackwrap(uint64_t arg0, uint64_t arg1 __attribute__((unused)), uint64_t arg2 __attribute__((unused)))
{
    int *rvalp = 0;
    if (arg0)
	rvalp = (int *) (uintptr_t) arg0;

    if (lock_try_acquire(&swapout_lock) < 0) {
	cprintf("pstate_sync: another sync still active\n");
	if (rvalp && !*rvalp)
	    *rvalp = -E_BUSY;
	return;
    }

    // If we don't have a valid header on disk, init the freelist
    if (stable_hdr.ph_magic != PSTATE_MAGIC) {
	uint64_t disk_pages = disk_bytes / PGSIZE;
	uint64_t reserved_pages = RESERVED_PAGES;
	assert(disk_pages > reserved_pages);

	if (pstate_swapout_debug)
	    cprintf("pstate_sync: %"PRIu64" disk pages\n", disk_pages);

	btree_manager_init();
	freelist_init(&freelist, reserved_pages * PGSIZE,
		      (disk_pages - reserved_pages) * PGSIZE);
    }

    static_assert(sizeof(struct pstate_header) == PSTATE_BUF_SIZE);

    static struct pstate_header pstate_scratch_buf;
    struct pstate_header *hdr = &pstate_scratch_buf;
    memcpy(hdr, &stable_hdr, sizeof(stable_hdr));

    hdr->ph_magic = PSTATE_MAGIC;
    hdr->ph_version = PSTATE_VERSION;
    hdr->ph_sync_ts = handle_alloc();
    hdr->ph_handle_counter = handle_counter;
    hdr->ph_user_root_handle = user_root_handle;
    hdr->ph_user_msec = timer_user_msec;
    memcpy(&hdr->ph_handle_key[0], &handle_key[0], HANDLE_KEY_SIZE);

    struct swapout_stats stats;
    memset(&stats, 0, sizeof(stats));

    struct kobject *ko, *ko_next;
    LIST_FOREACH(ko, &ko_list, ko_link) {
	stats.total_kobj++;
	if ((ko->hdr.ko_flags & KOBJ_DIRTY_LATER))
	    kobject_dirty_eval(ko);

	if ((ko->hdr.ko_flags & KOBJ_DIRTY)) {
	    kobject_snapshot(&ko->hdr);
	    ko->hdr.ko_flags |= KOBJ_SNAPSHOT_DIRTY;
	    stats.snapshoted_kobj++;
	}
    }

    int r = pstate_sync_loop(hdr, &stats);
    if (r < 0) {
	cprintf("pstate_sync_stackwrap: cannot sync: %s\n", e2s(r));

	// XXX flush btree cache?

	// Reset the un-committed log by applying the committed on-disk one.
	assert(0 == pstate_apply_disk_log());

	if (rvalp && !*rvalp)
	    *rvalp = r;
    }

    for (ko = LIST_FIRST(&ko_list); ko; ko = ko_next) {
	ko_next = LIST_NEXT(ko, ko_link);

	if ((ko->hdr.ko_flags & KOBJ_SNAPSHOT_DIRTY)) {
	    ko->hdr.ko_flags &= ~KOBJ_SNAPSHOT_DIRTY;
	    if (r < 0)
		ko->hdr.ko_flags |= KOBJ_DIRTY;
	    else
		ko->hdr.ko_sync_ts = hdr->ph_sync_ts;
	}

	if ((ko->hdr.ko_flags & KOBJ_SNAPSHOTING)) {
	    struct kobject *snap = kobject_get_snapshot(&ko->hdr);
	    kobject_snapshot_release(&ko->hdr);
    
	    if (r == 0 && snap->hdr.ko_type == kobj_dead)
		ko->hdr.ko_flags &= ~KOBJ_ON_DISK;
	}
    }

    if (pstate_swapout_stats) {
	cprintf("pstate_sync: total %"PRIu64" snap %"PRIu64" dead %"PRIu64" wrote %"PRIu64" pages %"PRIu64"\n",
		stats.total_kobj, stats.snapshoted_kobj, stats.dead_kobj,
		stats.written_kobj, stats.written_pages);
	cprintf("pstate_sync: pages used %"PRIu64" avail %"PRIu64" allocs %"PRIu64" fail %"PRIu64"\n",
		page_stats.pages_used, page_stats.pages_avail,
		page_stats.allocations, page_stats.failures);
    }

    while (!LIST_EMPTY(&swapout_waiting))
	thread_set_runnable(LIST_FIRST(&swapout_waiting));

    lock_release(&swapout_lock);
    if (rvalp && !*rvalp)
	*rvalp = 1;
}

static void
pstate_sync(void)
{
    int r = stackwrap_call(&pstate_sync_stackwrap, 0, 0, 0);
    if (r < 0)
	cprintf("pstate_sync: cannot stackwrap: %s\n", e2s(r));
}

int
pstate_sync_now(void)
{
    int rval = 0;
    int r = stackwrap_call(&pstate_sync_stackwrap, (uintptr_t) &rval, 0, 0);
    if (r < 0)
	return r;

    while (rval == 0)
	ide_intr();

    return rval < 0 ? rval : 0;
}

//////////////////////////////////////////////////
// User-initiated sync-to-disk
//////////////////////////////////////////////////

static void
pstate_sync_object_stackwrap(uint64_t arg, uint64_t start, uint64_t nbytes)
{
    // Casting to non-const, but it's OK here.
    struct kobject *ko = (struct kobject *) (uintptr_t) arg;

    thread_suspend(cur_thread, &swapout_waiting);
    if (lock_try_acquire(&swapout_lock) < 0) {
	if (pstate_swapout_object_debug)
	    cprintf("pstate_sync_object_stackwrap: waiting for swapout lock\n");
	return;
    }

    // kobject_snapshot() clears KOBJ_DIRTY, but we don't want that here..
    uint64_t dirty = (ko->hdr.ko_flags & KOBJ_DIRTY);
    kobject_snapshot(&ko->hdr);
    ko->hdr.ko_flags |= dirty;
    struct kobject *snap = kobject_get_snapshot(&ko->hdr);
    uint64_t sync_ts = handle_alloc();

    kobject_id_t id_found, id = snap->hdr.ko_id;
    struct mobject mobj;
    int r = btree_search(BTREE_OBJMAP, &id, &id_found, (uint64_t *) &mobj);
    if (r < 0) {
	if (r != -E_NOT_FOUND)
	    cprintf("pstate_sync_object_stackwrap: lookup: %s\n", e2s(r));
	goto fallback;
    }

    uint64_t sync_end = MIN(start + nbytes, snap->hdr.ko_nbytes);
    uint64_t req_disk_bytes = KOBJ_DISK_SIZE + ROUNDUP(sync_end, 512);
    if (req_disk_bytes > mobj.nbytes) {
	if (pstate_swapout_object_debug)
	    cprintf("pstate_sync_object_stackwrap: object grew, fallback\n");
	goto fallback;
    }

    struct pstate_iov_collector x;
    memset(&x, 0, sizeof(x));
    x.flush_off = mobj.off + KOBJ_DISK_SIZE + ROUNDDOWN(start, PGSIZE);
    x.flush_op = op_write;

    for (uint64_t page = start / PGSIZE; page < ROUNDUP(sync_end, PGSIZE) / PGSIZE; page++) {
	void *p;
	r = kobject_get_page(&snap->hdr, page, &p, page_shared_ro);
	if (r < 0)
	    goto fallback;

	uint32_t pagebytes = MIN(ROUNDUP(sync_end - page * PGSIZE, 512), (uint32_t) PGSIZE);
	r = pstate_iov_append(&x, p, pagebytes);
	if (r < 0)
	    goto fallback;
    }

    r = pstate_iov_flush(&x);
    if (r < 0)
	goto fallback;

    if (!SAFE_EQUAL(stackwrap_disk_io(op_flush, 0, 0, 0), disk_io_success))
	goto fallback;

    if (pstate_swapout_object_debug)
	cprintf("pstate_sync_object_stackwrap: incremental sync OK\n");
    ko->hdr.ko_sync_ts = sync_ts;

    while (!LIST_EMPTY(&swapout_waiting))
	thread_set_runnable(LIST_FIRST(&swapout_waiting));
    kobject_snapshot_release(&ko->hdr);
    lock_release(&swapout_lock);
    return;

fallback:
    kobject_snapshot_release(&ko->hdr);
    lock_release(&swapout_lock);
    pstate_sync();
}

int
pstate_sync_object(uint64_t timestamp, const struct kobject *ko,
		   uint64_t start, uint64_t nbytes)
{
    if (stable_hdr.ph_magic != PSTATE_MAGIC)
	goto fallback;

    if (ko->hdr.ko_sync_ts &&
	handle_decrypt(ko->hdr.ko_sync_ts) > handle_decrypt(timestamp))
	return 0;

    int r = stackwrap_call(&pstate_sync_object_stackwrap,
			   (uintptr_t) ko, start, nbytes);
    if (r < 0) {
	cprintf("pstate_sync_object: cannot stackwrap: %s\n", e2s(r));
	goto fallback;
    }

    return -E_RESTART;

fallback:
    thread_suspend(cur_thread, &swapout_waiting);
    pstate_sync();
    return -E_RESTART;
}

int
pstate_sync_user(uint64_t timestamp)
{
    if (stable_hdr.ph_magic == PSTATE_MAGIC &&
	handle_decrypt(stable_hdr.ph_sync_ts) > handle_decrypt(timestamp))
	return 0;

    thread_suspend(cur_thread, &swapout_waiting);
    pstate_sync();
    return -E_RESTART;
}

void
pstate_init(void)
{
    pstate_reset();

    static struct periodic_task sync_pt = { .pt_fn = &pstate_sync };
    sync_pt.pt_interval_ticks = kclock_hz * 3600;
    timer_add_periodic(&sync_pt);
}
