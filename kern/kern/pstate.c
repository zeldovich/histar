#include <machine/pmap.h>
#include <machine/thread.h>
#include <machine/x86.h>
#include <machine/stackwrap.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <kern/handle.h>
#include <kern/lib.h>
#include <inc/error.h>

// Print stats on how much we write to disk each round?
static int pstate_sync_stats = 0;

// Authoritative copy of the header that's actually on disk.
static struct pstate_header stable_hdr;

// Scratch-space for a copy of the header used while reading/writing.
#define N_HEADER_PAGES		3
#define PSTATE_BUF_SIZE		(N_HEADER_PAGES * PGSIZE)
static union {
    struct pstate_header hdr;
    char buf[PSTATE_BUF_SIZE];
} pstate_buf;

//////////////////////////////////
// State for init/sync FSM's
//////////////////////////////////
static struct {
    struct pstate_header *hdr;

    int cb;
    int slot;
    int done;
} state;

static struct {
    struct kobject *ko;
    uint64_t extra_page;
    int slot;
} swapin_state;

struct swapout_stats {
    uint64_t written_kobj;
    uint64_t written_pages;
    uint64_t snapshoted_kobj;
    uint64_t dead_kobj;
};

//////////////////////////////////
// Free list management
//////////////////////////////////
static void
freelist_init(struct pstate_free_list *f)
{
    // Mark the header pages as being in-use
    for (int i = 0; i < N_HEADER_PAGES; i++)
	f->inuse[i] = 1;
}

static int64_t
freelist_alloc(struct pstate_free_list *f, uint64_t npages)
{
    uint64_t base = 0;
    uint64_t nfound = 0;

    while (nfound < npages && base + nfound < NUM_PH_PAGES) {
	if (f->inuse[base + nfound]) {
	    base = base + nfound + 1;
	    nfound = 0;
	} else {
	    nfound++;
	}
    }

    if (nfound == npages) {
	for (uint64_t i = base; i < base + npages; i++)
	    f->inuse[i] = 1;
	return base;
    }

    return -E_NO_MEM;
}

static void
freelist_freelater(struct pstate_free_list *f, uint64_t base, uint64_t npages)
{
    for (uint64_t i = base; i < base + npages; i++)
	f->inuse[i] = 2;
}

static void
freelist_commit(struct pstate_free_list *f)
{
    for (uint64_t i = 0; i < NUM_PH_PAGES; i++)
	if (f->inuse[i] == 2)
	    f->inuse[i] = 0;
}

//////////////////////////////////////////////////
// Object map
//////////////////////////////////////////////////

static int
pstate_map_findslot(struct pstate_map *m, kobject_id_t id)
{
    for (int i = 0; i < NUM_PH_OBJECTS; i++)
	if (m->ent[i].id == id && m->ent[i].offset != 0)
	    return i;
    return -1;
}

static void
pstate_kobj_free(struct pstate_map *m, struct pstate_free_list *f,
		 struct kobject *ko)
{
    int slot = pstate_map_findslot(m, ko->u.hdr.ko_id);
    if (slot < 0)
	return;

    freelist_freelater(f, m->ent[slot].offset, m->ent[slot].pages);
    m->ent[slot].offset = 0;
}

static int
pstate_kobj_alloc(struct pstate_map *m, struct pstate_free_list *f,
		  struct kobject *ko)
{
    pstate_kobj_free(m, f, ko);

    for (int i = 0; i < NUM_PH_OBJECTS; i++) {
	if (m->ent[i].offset == 0) {
	    uint64_t npages = ko->u.hdr.ko_npages + 1;
	    int64_t offset = freelist_alloc(f, npages);
	    if (offset < 0) {
		cprintf("pstate_kobj_alloc: no room for %ld pages\n", npages);
		return offset;
	    }

	    m->ent[i].id = ko->u.hdr.ko_id;
	    m->ent[i].type = ko->u.hdr.ko_type;
	    m->ent[i].flags = ko->u.hdr.ko_flags;
	    m->ent[i].offset = offset;
	    m->ent[i].pages = npages;

	    if (ko->u.hdr.ko_ref == 0)
		m->ent[i].flags |= KOBJ_ZERO_REFS;

	    return i;
	}
    }

    return -1;
}

//////////////////////////////////
// Swap-in code
//////////////////////////////////

static void
swapin_kobj_cb(disk_io_status stat, void *buf,
	       uint32_t count, uint64_t offset, void *arg)
{
    void (*cb)(int) = (void (*)(int)) arg;

    if (stat != disk_io_success) {
	cprintf("swapin_kobj_cb: disk IO failure\n");
	(*cb) (-1);
	return;
    }

    if (swapin_state.extra_page == 0) {
	for (int i = 0; i < KOBJ_DIRECT_PAGES; i++)
	    swapin_state.ko->u.hdr.ko_pages[i] = 0;
	swapin_state.ko->u.hdr.ko_pages_indir1 = 0;
    }

    if (swapin_state.extra_page > 0)
	kobject_swapin_page(swapin_state.ko,
			    swapin_state.extra_page - 1,
			    buf);

    if (swapin_state.extra_page < swapin_state.ko->u.hdr.ko_npages) {
	void *p;
	int r = page_alloc(&p);
	if (r < 0) {
	    cprintf("init_kobj_cb: cannot alloc page: %s\n", e2s(r));
	    (*cb) (r);
	    return;
	}

	uint64_t offset = (stable_hdr.ph_map.ent[swapin_state.slot].offset +
			   swapin_state.extra_page + 1) * PGSIZE;
	++swapin_state.extra_page;

	state.cb = 4;
	r = disk_io(op_read, p, PGSIZE, offset, &swapin_kobj_cb, arg);
	if (r < 0) {
	    cprintf("swapin_kobj_cb: cannot submit disk IO: %s\n", e2s(r));
	    (*cb) (r);
	}
    } else {
	kobject_swapin(swapin_state.ko);

	swapin_state.ko = 0;
	(*cb) (0);
    }
}

static void
swapin_kobj(int slot, void (*cb)(int)) {
    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("swapin_kobj: cannot alloc page: %s\n", e2s(r));
	(*cb) (r);
	return;
    }

    if (swapin_state.ko)
	panic("swapin_kobj: still active!");

    swapin_state.ko = (struct kobject *) p;
    swapin_state.extra_page = 0;
    swapin_state.slot = slot;
    state.cb = 5;
    r = disk_io(op_read, p, PGSIZE,
		stable_hdr.ph_map.ent[slot].offset * PGSIZE,
		&swapin_kobj_cb, (void*) cb);
    if (r < 0) {
	cprintf("swapin_kobj: cannot submit disk IO: %s\n", e2s(r));
	(*cb) (r);
    }
}

static struct Thread_list swapin_waiting;

static void
pstate_swapin_cb(int r)
{
    if (r < 0)
	cprintf("pstate_swapin_cb: %s\n", e2s(r));

    while (!LIST_EMPTY(&swapin_waiting)) {
	struct Thread *t = LIST_FIRST(&swapin_waiting);
	thread_set_runnable(t);
    }
}

int
pstate_swapin(kobject_id_t id) {
    //cprintf("pstate_swapin: object %ld\n", id);

    int slot = pstate_map_findslot(&stable_hdr.ph_map, id);
    if (slot < 0)
	return -E_INVAL;

    if (cur_thread)
	thread_suspend(cur_thread, &swapin_waiting);

    if (swapin_state.ko == 0)
	swapin_kobj(slot, &pstate_swapin_cb);
    return 0;
}

/////////////////////////////////////
// Persistent-store initialization
/////////////////////////////////////

static void
init_done(void)
{
    handle_counter = stable_hdr.ph_handle_counter;
    user_root_handle = stable_hdr.ph_user_root_handle;
    state.done = 1;
}

static void init_kobj();

static void
init_kobj_cb(int r)
{
    if (r < 0) {
	cprintf("init_kobj_cb: error swapping in: %s\n", e2s(r));
	state.done = r;
	return;
    }

    ++state.slot;
    init_kobj();
}

static void
init_kobj(void)
{
    // Page in all threads and pinned objects, but demand-page the rest
    while (state.slot < NUM_PH_OBJECTS) {
	if (stable_hdr.ph_map.ent[state.slot].offset == 0) {
	    ++state.slot;
	    continue;
	}

	if (stable_hdr.ph_map.ent[state.slot].flags & KOBJ_PIN_IDLE)
	    break;
	if (stable_hdr.ph_map.ent[state.slot].flags & KOBJ_ZERO_REFS)
	    break;
	if (stable_hdr.ph_map.ent[state.slot].type == kobj_thread)
	    break;

	++state.slot;
    }

    if (state.slot == NUM_PH_OBJECTS) {
	init_done();
	return;
    }

    swapin_kobj(state.slot, &init_kobj_cb);
}

static void
init_hdr_cb(disk_io_status stat, void *buf,
	    uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    memcpy(&stable_hdr, &pstate_buf.hdr, sizeof(stable_hdr));
    if (stable_hdr.ph_magic != PSTATE_MAGIC ||
	stable_hdr.ph_version != PSTATE_VERSION)
    {
	cprintf("pstate_init_hdr: magic/version mismatch\n");
	state.done = -E_INVAL;
	return;
    }

    state.slot = 0;
    init_kobj();
}

void
pstate_reset(void)
{
    memset(&stable_hdr, 0, sizeof(stable_hdr));
    freelist_init(&stable_hdr.ph_free);
}

int
pstate_init(void)
{
    LIST_INIT(&swapin_waiting);

    state.done = 0;
    state.cb = 6;
    int r = disk_io(op_read, &pstate_buf.buf[0], PSTATE_BUF_SIZE, 0, &init_hdr_cb, 0);
    if (r < 0) {
	cprintf("pstate_init: cannot submit disk IO\n");
	return r;
    }

    uint64_t ts_start = read_tsc();
    int warned = 0;
    while (!state.done) {
	uint64_t ts_now = read_tsc();
	if (warned == 0 && ts_now - ts_start > 1024*1024*1024) {
	    cprintf("pstate_init: wedged for %ld, cb %d\n",
		    ts_now - ts_start, state.cb);
	    warned = 1;
	}
	ide_intr();
    }

    return state.done;
}

//////////////////////////////////////////////////
// Swap-out code
//////////////////////////////////////////////////

static int
pstate_sync_kobj(struct pstate_header *hdr,
		 struct swapout_stats *stats,
		 struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    int slot = pstate_kobj_alloc(&hdr->ph_map, &hdr->ph_free, snap);
    if (slot < 0) {
	cprintf("pstate_sync_kobj: cannot allocate space: %s\n", e2s(slot));
	return -1;
    }

    disk_io_status s =
	stackwrap_disk_io(op_write, snap, sizeof(*snap),
			  hdr->ph_map.ent[slot].offset * PGSIZE);
    if (s != disk_io_success) {
	cprintf("pstate_sync_kobj: error during disk io\n");
	return -1;
    }

    for (uint64_t page = 0; page < snap->u.hdr.ko_npages; page++) {
	uint64_t offset = (hdr->ph_map.ent[slot].offset + page + 1) * PGSIZE;
	void *p;
	int r = kobject_get_page(&snap->u.hdr, page, &p, kobj_ro);
	if (r < 0)
	    panic("pstate_sync_kobj: cannot get page: %s", e2s(r));

	s = stackwrap_disk_io(op_write, p, PGSIZE, offset);
	if (s != disk_io_success) {
	    cprintf("pstate_sync_kobj: error during disk io for page\n");
	    return -1;
	}

	stats->written_pages++;
    }

    kobject_snapshot_release(ko);
    stats->written_kobj++;
    return 0;
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
	if (snap->u.hdr.ko_type == kobj_dead) {
	    pstate_kobj_free(&hdr->ph_map, &hdr->ph_free, snap);
	    stats->dead_kobj++;
	    continue;
	}

	int r = pstate_sync_kobj(hdr, stats, ko);
	if (r < 0)
	    return r;
    }

    freelist_commit(&hdr->ph_free);
    disk_io_status s = stackwrap_disk_io(op_write, hdr, PSTATE_BUF_SIZE, 0);
    if (s == disk_io_success) {
	memcpy(&stable_hdr, hdr, sizeof(stable_hdr));
	return 0;
    } else {
	cprintf("pstate_sync_stackwrap: error writing header\n");
	return -1;
    }
}

static void
pstate_sync_stackwrap(void *arg)
{
    static int swapout_active;

    if (swapout_active) {
	cprintf("pstate_sync: another sync still active\n");
	return;
    }

    swapout_active = 1;

    static_assert(sizeof(pstate_buf.hdr) <= PSTATE_BUF_SIZE);
    struct pstate_header *hdr = &pstate_buf.hdr;
    memcpy(hdr, &stable_hdr, sizeof(stable_hdr));
    hdr->ph_magic = PSTATE_MAGIC;
    hdr->ph_version = PSTATE_VERSION;
    hdr->ph_handle_counter = handle_counter;
    hdr->ph_user_root_handle = user_root_handle;

    struct swapout_stats stats;
    memset(&stats, 0, sizeof(stats));

    struct kobject_hdr *ko, *ko_next;
    LIST_FOREACH(ko, &ko_list, ko_link) {
	if ((ko->ko_flags & KOBJ_DIRTY)) {
	    kobject_snapshot(ko);
	    stats.snapshoted_kobj++;
	}
    }

    int r = pstate_sync_loop(hdr, &stats);
    if (r < 0)
	cprintf("pstate_sync_stackwrap: cannot sync\n");

    for (ko = LIST_FIRST(&ko_list); ko; ko = ko_next) {
	ko_next = LIST_NEXT(ko, ko_link);

	if ((ko->ko_flags & KOBJ_SNAPSHOTING)) {
	    struct kobject *snap = kobject_get_snapshot(ko);
	    kobject_snapshot_release(ko);

	    if (snap->u.hdr.ko_type == kobj_dead)
		kobject_swapout(kobject_h2k(ko));
	}
    }

    if (pstate_sync_stats)
	cprintf("pstate_sync: snap %ld dead %ld wrote %ld pages %ld\n",
		stats.snapshoted_kobj, stats.dead_kobj,
		stats.written_kobj, stats.written_pages);
    swapout_active = 0;
}

void
pstate_sync(void)
{
    int r = stackwrap_call(&pstate_sync_stackwrap, 0);
    if (r < 0)
	cprintf("pstate_sync: cannot stackwrap: %s\n", e2s(r));
}
