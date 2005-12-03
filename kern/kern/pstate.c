#include <machine/pmap.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/lib.h>

static char pstate_buf[PGSIZE] __attribute__((__aligned__ (PGSIZE)));

static struct {
    struct pstate_header *hdr;

    uint64_t disk_page;
    int map_slot;

    struct kobject *ko;
    int extra_page;

    int done;
} state;

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

    if (state.extra_page == 0) {
	state.ko = buf;
	kobject_swapin(state.ko);
    } else {
	kobject_swapin_page(state.ko, state.extra_page - 1, buf);
    }

    if (state.extra_page < state.ko->ko_extra_pages) {
	void *p;
	int r = page_alloc(&p);
	if (r < 0) {
	    cprintf("init_kobj_cb: cannot alloc page: %d\n", r);
	    state.done = -1;
	    return;
	}

	uint64_t offset = state.extra_page * PGSIZE;
	state.extra_page++;
	disk_io(op_read, p, PGSIZE,
		state.hdr->ph_map[state.map_slot].offset + offset,
		&init_kobj_cb, 0);
    } else {
	state.map_slot++;
	init_kobj();
    }
}

static void
init_kobj()
{
    if (state.hdr->ph_map[state.map_slot].offset == 0) {
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

    state.extra_page = 0;
    disk_io(op_read, p, PGSIZE, state.hdr->ph_map[state.map_slot].offset, &init_kobj_cb, 0);
}

static void
init_hdr_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	state.done = -1;
	return;
    }

    state.hdr = (struct pstate_header *) buf;
    if (state.hdr->ph_magic != PSTATE_MAGIC ||
	state.hdr->ph_version != PSTATE_VERSION) {
	cprintf("pstate_init_hdr: magic/version mismatch\n");
	state.done = -1;
	return;
    }

    state.map_slot = 0;
    init_kobj();
}

int
pstate_init()
{
    // XXX disable for now, doesn't quite work yet
    if (1)
	return -1;

    state.done = 0;
    disk_io(op_read, &pstate_buf[0], PGSIZE, 0, &init_hdr_cb, 0);
    while (!state.done)
	ide_intr();

    return state.done;
}

//////////////////////////////////////////////////
// Swap-out code
//////////////////////////////////////////////////

static void
sync_header_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_header_cb: io failure\n");
	state.done = -1;
	return;
    }

    //cprintf("sync_header_cb: all done, %ld pages, %d objects\n", state.disk_page, state.map_slot);
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

    if (state.ko->ko_extra_pages > state.extra_page) {
	uint64_t offset = state.disk_page * PGSIZE;
	state.disk_page++;

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
    if (state.ko == 0) {
	state.hdr->ph_map[state.map_slot].offset = 0;
	disk_io(op_write, state.hdr, PGSIZE, 0, sync_header_cb, 0);
    } else {
	if (state.map_slot > NUM_PH_OBJECTS) {
	    cprintf("sync_kobj: too many objects\n");
	    state.done = -1;
	    return;
	}

	uint64_t offset = state.disk_page * PGSIZE;
	state.hdr->ph_map[state.map_slot].id = state.ko->ko_id;
	state.hdr->ph_map[state.map_slot].offset = offset;

	state.map_slot++;
	state.disk_page++;
	state.extra_page = 0;

	disk_io(op_write, state.ko, PGSIZE, offset, sync_kobj_cb, 0);
    }
}

static void
sync_hdr_zero(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_hdr_inval: io error\n");
	state.done = -1;
	return;
    }

    state.hdr = (void *) &pstate_buf[0];
    state.hdr->ph_magic = PSTATE_MAGIC;
    state.hdr->ph_version = PSTATE_VERSION;
    state.hdr->ph_handle_counter = handle_counter;

    state.disk_page = 1;
    state.map_slot = 0;
    state.ko = LIST_FIRST(&ko_list);

    state.done = 0;
    sync_kobj();
}

void
pstate_sync()
{
    memset(&pstate_buf[0], 0, PGSIZE);
    disk_io(op_write, &pstate_buf[0], PGSIZE, 0, sync_hdr_zero, 0);
    while (!state.done)
	ide_intr();
}
