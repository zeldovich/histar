#include <machine/pmap.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/lib.h>
#include <inc/error.h>

// Align to avoid IDE-DMA problems (can't cross 64K boundary)
static union __attribute__ ((__aligned__ (2*PGSIZE))) {
    struct pstate_header hdr;
    char buf[2*PGSIZE];
} pstate_buf;

//////////////////////////////////
// State for init/sync FSM's
//////////////////////////////////
static struct {
    struct pstate_header *hdr;

    struct kobject *ko;
    int slot;
    int extra_page;

    int done;
} state;

//////////////////////////////////
// Free list management
//////////////////////////////////
static void
freelist_init(struct pstate_free_list *f)
{
    // Mark the header pages as being in-use
    f->inuse[0] = 1;
    f->inuse[1] = 1;
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

//////////////////////////////////
// Swap-in code
//////////////////////////////////
static void
init_done()
{
    handle_counter = state.hdr->ph_handle_counter;
    state.done = 1;
}

static void init_kobj();

static void
init_kobj_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    if (state.extra_page > 0)
	kobject_swapin_page(state.ko, state.extra_page - 1, buf);

    if (state.extra_page < state.ko->ko_extra_pages) {
	void *p;
	int r = page_alloc(&p);
	if (r < 0) {
	    cprintf("init_kobj_cb: cannot alloc page: %d\n", r);
	    state.done = -1;
	    return;
	}

	uint64_t offset = (state.hdr->ph_map.ent[state.slot].offset + state.extra_page + 1) * PGSIZE;
	++state.extra_page;

	disk_io(op_read, p, PGSIZE, offset, &init_kobj_cb, 0);
    } else {
	kobject_swapin(state.ko);
	++state.slot;
	init_kobj();
    }
}

static void
init_kobj()
{
    // XXX not impl yet
    // Page in all threads and pinned objects, but demand-page the rest
    while (state.slot < NUM_PH_OBJECTS) {
	if (state.hdr->ph_map.ent[state.slot].offset != 0)
	    break;

	++state.slot;
    }

    if (state.slot == NUM_PH_OBJECTS) {
	init_done();
	return;
    }

    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("init_kobj: cannot alloc page: %d\n", r);
	state.done = -1;
	return;
    }

    state.ko = p;
    state.extra_page = 0;
    disk_io(op_read, p, PGSIZE, state.hdr->ph_map.ent[state.slot].offset * PGSIZE, &init_kobj_cb, 0);
}

static void
init_hdr_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    state.hdr = &pstate_buf.hdr;
    if (state.hdr->ph_magic != PSTATE_MAGIC ||
	state.hdr->ph_version != PSTATE_VERSION)
    {
	cprintf("pstate_init_hdr: magic/version mismatch\n");

	memset(&pstate_buf.hdr, 0, sizeof(pstate_buf.hdr));
	freelist_init(&pstate_buf.hdr.ph_free);
	state.done = -1;
	return;
    }

    state.slot = 0;
    init_kobj();
}

int
pstate_init()
{
    memset(&pstate_buf.hdr, 0, sizeof(pstate_buf.hdr));
    state.done = 0;
    disk_io(op_read, &pstate_buf.buf[0], 2*PGSIZE, 0, &init_hdr_cb, 0);
    while (!state.done)
	ide_intr();

    return state.done;
}

//////////////////////////////////////////////////
// Swap-out code
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
pstate_kobj_free(struct pstate_map *m, struct pstate_free_list *f, struct kobject *ko)
{
    int slot = pstate_map_findslot(m, ko->ko_id);
    if (slot < 0)
	return;

    freelist_freelater(f, m->ent[slot].offset, m->ent[slot].pages);
    m->ent[slot].offset = 0;
}

static int
pstate_kobj_alloc(struct pstate_map *m, struct pstate_free_list *f, struct kobject *ko)
{
    pstate_kobj_free(m, f, ko);

    for (int i = 0; i < NUM_PH_OBJECTS; i++) {
	if (m->ent[i].offset == 0) {
	    uint64_t npages = ko->ko_extra_pages + 1;
	    int64_t offset = freelist_alloc(f, npages);
	    if (offset < 0) {
		cprintf("pstate_kobj_alloc: no room for %ld pages\n", npages);
		return offset;
	    }

	    m->ent[i].id = ko->ko_id;
	    m->ent[i].type = ko->ko_type;
	    m->ent[i].flags = ko->ko_flags;
	    m->ent[i].offset = offset;
	    m->ent[i].pages = npages;
	    return i;
	}
    }

    return -1;
}

static void
sync_header_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_header_cb: io failure\n");
	state.done = -1;
	return;
    }

    state.done = 1;
}

static void sync_kobj();

static void
sync_kobj_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_kobj_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    if (state.extra_page < state.ko->ko_extra_pages) {
	uint64_t offset = (state.hdr->ph_map.ent[state.slot].offset + state.extra_page + 1) * PGSIZE;
	void *p = kobject_swapout_page(state.ko, state.extra_page);
	state.extra_page++;

	disk_io(op_write, p, PGSIZE, offset, sync_kobj_cb, 0);
    } else {
	state.ko = LIST_NEXT(state.ko, ko_link);
	sync_kobj();
    }
}

static void
sync_kobj()
{
    while (state.ko && state.ko->ko_type == kobj_dead) {
	pstate_kobj_free(&state.hdr->ph_map, &state.hdr->ph_free, state.ko);
	state.ko = LIST_NEXT(state.ko, ko_link);
    }

    if (state.ko == 0) {
	freelist_commit(&state.hdr->ph_free);
	disk_io(op_write, state.hdr, 2*PGSIZE, 0, sync_header_cb, 0);
	return;
    }

    state.slot = pstate_kobj_alloc(&state.hdr->ph_map, &state.hdr->ph_free, state.ko);
    if (state.slot < 0) {
	cprintf("sync_kobj: cannot allocate space\n");
	state.done = -1;
	return;
    }

    state.extra_page = 0;
    disk_io(op_write, state.ko, PGSIZE, state.hdr->ph_map.ent[state.slot].offset * PGSIZE, sync_kobj_cb, 0);
}

void
pstate_sync()
{
    static_assert(sizeof(pstate_buf.hdr) <= 2*PGSIZE);
    state.hdr = &pstate_buf.hdr;
    state.hdr->ph_magic = PSTATE_MAGIC;
    state.hdr->ph_version = PSTATE_VERSION;
    state.hdr->ph_handle_counter = handle_counter;

    state.ko = LIST_FIRST(&ko_list);
    state.done = 0;
    sync_kobj();

    while (!state.done)
	ide_intr();
}
