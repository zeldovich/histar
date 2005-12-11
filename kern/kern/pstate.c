#include <machine/pmap.h>
#include <machine/thread.h>
#include <machine/x86.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/lib.h>
#include <inc/error.h>

// Authoritative copy of the header that's actually on disk.
static struct pstate_header stable_hdr;

// Scratch-space for a copy of the header used while reading/writing.
static union {
    struct pstate_header hdr;
    char buf[2*PGSIZE];
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
    int extra_page;
} swapout_state;

static struct {
    struct kobject *ko;
    int extra_page;
    int slot;
} swapin_state;

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
    int slot = pstate_map_findslot(m, ko->ko_id);
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
	    uint64_t npages = ko->ko_npages + 1;
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

	    if (ko->ko_ref == 0)
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
	    swapin_state.ko->ko_pages[i] = 0;
	swapin_state.ko->ko_pages_indir1 = 0;
    }

    if (swapin_state.extra_page > 0)
	kobject_swapin_page(swapin_state.ko,
			    swapin_state.extra_page - 1,
			    buf);

    if (swapin_state.extra_page < swapin_state.ko->ko_npages) {
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

    swapin_state.ko = p;
    swapin_state.extra_page = 0;
    swapin_state.slot = slot;
    state.cb = 5;
    r = disk_io(op_read, p, PGSIZE,
		stable_hdr.ph_map.ent[slot].offset * PGSIZE,
		&swapin_kobj_cb, cb);
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
init_done()
{
    handle_counter = stable_hdr.ph_handle_counter;
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
init_kobj()
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

int
pstate_init()
{
    LIST_INIT(&swapin_waiting);

    state.done = 0;
    state.cb = 6;
    int r = disk_io(op_read, &pstate_buf.buf[0], 2*PGSIZE, 0, &init_hdr_cb, 0);
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

    if (state.done < 0) {
	memset(&stable_hdr, 0, sizeof(stable_hdr));
	freelist_init(&stable_hdr.ph_free);
    }

    return state.done;
}

//////////////////////////////////////////////////
// Swap-out code
//////////////////////////////////////////////////

static void
sync_header_cb(disk_io_status stat, void *buf,
	       uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_header_cb: io failure\n");
	state.done = -1;
	return;
    }

    memcpy(&stable_hdr, state.hdr, sizeof(stable_hdr));
    state.done = 1;
}

static void sync_kobj();

static void
sync_kobj_cb(disk_io_status stat, void *buf,
	     uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_kobj_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    if (swapout_state.extra_page < swapout_state.ko->ko_npages) {
	uint64_t offset = (state.hdr->ph_map.ent[state.slot].offset +
			   swapout_state.extra_page + 1) * PGSIZE;
	void *p;
	int r = kobject_get_page(swapout_state.ko,
				 swapout_state.extra_page, &p);
	if (r < 0)
	    panic("sync_kobj_cb: cannot get object page");

	swapout_state.extra_page++;
	state.cb = 1;
	r = disk_io(op_write, p, PGSIZE, offset, sync_kobj_cb, 0);
	if (r < 0) {
	    cprintf("sync_kobj_cb: cannot submit disk IO: %s\n", e2s(r));
	    state.done = r;
	    return;
	}
    } else {
	swapout_state.ko = LIST_NEXT(swapout_state.ko, ko_link);
	sync_kobj();
    }
}

static void
sync_kobj()
{
    while (swapout_state.ko && swapout_state.ko->ko_type == kobj_dead) {
	pstate_kobj_free(&state.hdr->ph_map,
			 &state.hdr->ph_free, swapout_state.ko);
	kobject_swapout(swapout_state.ko);
	swapout_state.ko = LIST_NEXT(swapout_state.ko, ko_link);
    }

    if (swapout_state.ko == 0) {
	freelist_commit(&state.hdr->ph_free);
	state.cb = 2;
	int r = disk_io(op_write, state.hdr, 2*PGSIZE, 0, sync_header_cb, 0);
	if (r < 0) {
	    cprintf("sync_kobj: cannot submit disk IO: %s\n", e2s(r));
	    state.done = r;
	}

	return;
    }

    state.slot = pstate_kobj_alloc(&state.hdr->ph_map,
				   &state.hdr->ph_free, swapout_state.ko);
    if (state.slot < 0) {
	cprintf("sync_kobj: cannot allocate space\n");
	state.done = -1;
	return;
    }

    swapout_state.extra_page = 0;
    state.cb = 3;
    int r = disk_io(op_write, swapout_state.ko, PGSIZE,
		    state.hdr->ph_map.ent[state.slot].offset * PGSIZE,
		    sync_kobj_cb, 0);
    if (r < 0) {
	cprintf("sync_kobj: cannot submit disk IO: %s\n", e2s(r));
	state.done = r;
    }
}

void
pstate_sync()
{
    static_assert(sizeof(pstate_buf.hdr) <= 2*PGSIZE);
    state.hdr = &pstate_buf.hdr;
    memcpy(state.hdr, &stable_hdr, sizeof(stable_hdr));

    state.hdr->ph_magic = PSTATE_MAGIC;
    state.hdr->ph_version = PSTATE_VERSION;
    state.hdr->ph_handle_counter = handle_counter;

    state.done = 0;
    state.cb = 0;
    swapout_state.ko = LIST_FIRST(&ko_list);
    sync_kobj();

    uint64_t ts_start = read_tsc();
    int warned = 0;
    while (!state.done) {
	uint64_t ts_now = read_tsc();
	if (warned == 0 && ts_now - ts_start > 1024*1024*1024) {
	    cprintf("pstate_sync: wedged for %ld, cb %d\n",
		    ts_now - ts_start, state.cb);
	    warned = 1;
	}
	ide_intr();
    }
}
