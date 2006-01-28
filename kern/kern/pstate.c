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

// verbose flags
static int pstate_init_debug = 0;
static int pstate_swapin_debug = 0;
static int pstate_swapout_debug = 0;
static int pstate_swapout_stats = 0;

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

static int swapin_active;

static int
pstate_swapin_slot(int slot)
{
    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("pstate_swapin_slot: cannot alloc page: %s\n", e2s(r));
	return r;
    }

    assert(swapin_active == 0);
    swapin_active = 1;

    struct kobject *ko = (struct kobject *) p;
    uint64_t offset = stable_hdr.ph_map.ent[slot].offset * PGSIZE;
    disk_io_status s = stackwrap_disk_io(op_read, p, PGSIZE, offset);
    if (s != disk_io_success) {
	cprintf("pstate_swapin_slot: cannot read object from disk\n");
	swapin_active = 0;
	return -1;
    }

    pagetree_init(&ko->u.hdr.ko_pt);
    for (uint64_t page = 0; page < ko->u.hdr.ko_npages; page++) {
	r = page_alloc(&p);
	if (r < 0) {
	    cprintf("pstate_swapin_slot: cannot alloc page: %s\n", e2s(r));
	    swapin_active = 0;
	    return r;
	}

	offset = (stable_hdr.ph_map.ent[slot].offset + page + 1) * PGSIZE;
	s = stackwrap_disk_io(op_read, p, PGSIZE, offset);
	if (s != disk_io_success) {
	    cprintf("pstate_swapin_slot: cannot read page from disk\n");
	    swapin_active = 0;
	    return -1;
	}

	assert(0 == pagetree_put_page(&ko->u.hdr.ko_pt, page, p));
    }

    if (pstate_swapin_debug)
	cprintf("pstate_swapin_slot: slot %d id %ld npages %ld\n",
		slot, ko->u.hdr.ko_id, ko->u.hdr.ko_npages);

    kobject_swapin(ko);
    swapin_active = 0;
    return 0;
}

static void
pstate_swapin_stackwrap(void *arg)
{
    static struct Thread_list swapin_waiting;

    if (cur_thread)
	thread_suspend(cur_thread, &swapin_waiting);

    if (swapin_active)
	return;

    int slot = (int64_t) arg;
    int r = pstate_swapin_slot(slot);
    if (r < 0)
	cprintf("pstate_swapin_cb: %s\n", e2s(r));

    while (!LIST_EMPTY(&swapin_waiting)) {
	struct Thread *t = LIST_FIRST(&swapin_waiting);
	thread_set_runnable(t);
    }
}

int
pstate_swapin(kobject_id_t id) {
    if (pstate_swapin_debug)
	cprintf("pstate_swapin: object %ld\n", id);

    int64_t slot = pstate_map_findslot(&stable_hdr.ph_map, id);
    if (slot < 0)
	return -E_INVAL;

    int r = stackwrap_call(&pstate_swapin_stackwrap, (void *) slot);
    if (r < 0) {
	cprintf("pstate_swapin: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    return 0;
}

/////////////////////////////////////
// Persistent-store initialization
/////////////////////////////////////

static int
pstate_init2()
{
    disk_io_status s = stackwrap_disk_io(op_read, &pstate_buf.buf[0], PSTATE_BUF_SIZE, 0);
    if (s != disk_io_success) {
	cprintf("pstate_init2: cannot read header\n");
	return -1;
    }

    memcpy(&stable_hdr, &pstate_buf.hdr, sizeof(stable_hdr));
    if (stable_hdr.ph_magic != PSTATE_MAGIC ||
	stable_hdr.ph_version != PSTATE_VERSION)
    {
	cprintf("pstate_init_hdr: magic/version mismatch\n");
	return -E_INVAL;
    }

    for (int slot = 0; slot < NUM_PH_OBJECTS; slot++) {
	if (stable_hdr.ph_map.ent[slot].offset == 0)
	    continue;

	if (pstate_init_debug)
	    cprintf("pstate_init2: slot %d flags 0x%lx type %d\n",
		    slot,
		    stable_hdr.ph_map.ent[slot].flags,
		    stable_hdr.ph_map.ent[slot].type);

	if ((stable_hdr.ph_map.ent[slot].flags & KOBJ_PIN_IDLE) ||
	    (stable_hdr.ph_map.ent[slot].flags & KOBJ_ZERO_REFS) ||
	    (stable_hdr.ph_map.ent[slot].type == kobj_thread))
	{
	    if (pstate_init_debug)
		cprintf("pstate_init2: paging in slot %d\n", slot);

	    int r = pstate_swapin_slot(slot);
	    if (r < 0) {
		cprintf("pstate_init2: cannot swapin slot %d: %s\n",
			slot, e2s(r));
		return r;
	    }
	}
    }

    handle_counter   = stable_hdr.ph_handle_counter;
    user_root_handle = stable_hdr.ph_user_root_handle;

    if (pstate_init_debug)
	cprintf("pstate_init2: handle_counter %ld root_handle %ld\n",
		handle_counter, user_root_handle);

    return 1;
}

static void
pstate_init_stackwrap(void *arg)
{
    int *donep = (int *) arg;

    *donep = pstate_init2();
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
    pstate_reset();

    int done = 0;
    int r = stackwrap_call(&pstate_init_stackwrap, &done);
    if (r < 0) {
	cprintf("pstate_init: cannot stackwrap: %s\n", e2s(r));
	return r;
    }

    uint64_t ts_start = read_tsc();
    int warned = 0;
    while (!done) {
	uint64_t ts_now = read_tsc();
	if (warned == 0 && ts_now - ts_start > 1024*1024*1024) {
	    cprintf("pstate_init: wedged for %ld\n", ts_now - ts_start);
	    warned = 1;
	}
	ide_intr();
    }

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
};

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
	int r = kobject_get_page(&snap->u.hdr, page, &p, page_ro);
	if (r < 0)
	    panic("pstate_sync_kobj: cannot get page: %s", e2s(r));

	s = stackwrap_disk_io(op_write, p, PGSIZE, offset);
	if (s != disk_io_success) {
	    cprintf("pstate_sync_kobj: error during disk io for page\n");
	    return -1;
	}

	stats->written_pages++;
    }

    if (pstate_swapout_debug)
	cprintf("pstate_sync_kobj: id %ld npages %ld\n",
		snap->u.hdr.ko_id, snap->u.hdr.ko_npages);

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

    if (pstate_swapout_stats)
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
