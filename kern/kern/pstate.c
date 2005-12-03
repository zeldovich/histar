#include <machine/pmap.h>
#include <dev/disk.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/lib.h>

static char pstate_buf[PGSIZE] __attribute__((__aligned__ (PGSIZE)));
static volatile int pstate_done;

static struct {
    struct pstate_header *hdr;

    int map_slot;

    struct kobject *ko;
    int extra_page;
} init_state;

static void
init_done()
{
    handle_counter = init_state.hdr->ph_handle_counter;
    pstate_done = 1;
}

static void init_kobj();

static void
init_kobj_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	pstate_done = -1;
	return;
    }

    if (init_state.extra_page == 0) {
	init_state.ko = buf;
	kobject_swapin(init_state.ko);
    } else {
	kobject_swapin_page(init_state.ko, init_state.extra_page - 1, buf);
    }

    if (init_state.extra_page < init_state.ko->ko_extra_pages) {
	void *p;
	int r = page_alloc(&p);
	if (r < 0) {
	    cprintf("init_kobj_cb: cannot alloc page: %d\n", r);
	    pstate_done = -1;
	    return;
	}

	uint64_t offset = init_state.extra_page * PGSIZE;
	init_state.extra_page++;
	disk_io(op_read, p, PGSIZE,
		init_state.hdr->ph_map[init_state.map_slot].offset + offset,
		&init_kobj_cb, 0);
    } else {
	init_state.map_slot++;
	init_kobj();
    }
}

static void
init_kobj()
{
    if (init_state.hdr->ph_map[init_state.map_slot].offset == 0) {
	init_done();
	return;
    }

    void *p;
    int r = page_alloc(&p);
    if (r < 0) {
	cprintf("init_kobj: cannot alloc page: %d\n", r);
	pstate_done = -1;
	return;
    }

    init_state.extra_page = 0;
    disk_io(op_read, p, PGSIZE, init_state.hdr->ph_map[init_state.map_slot].offset, &init_kobj_cb, 0);
}

static void
init_hdr_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("pstate_init_cb: disk IO failure\n");
	pstate_done = -1;
	return;
    }

    init_state.hdr = (struct pstate_header *) buf;
    if (init_state.hdr->ph_magic != PSTATE_MAGIC ||
	init_state.hdr->ph_version != PSTATE_VERSION) {
	cprintf("pstate_init_hdr: magic/version mismatch\n");
	pstate_done = -1;
	return;
    }

    init_state.map_slot = 0;
    init_kobj();
}

int
pstate_init()
{
    // XXX disable for now, doesn't quite work yet
    if (1)
	return -1;

    pstate_done = 0;
    disk_io(op_read, &pstate_buf[0], PGSIZE, 0, &init_hdr_cb, 0);
    while (!pstate_done)
	ide_intr();

    return pstate_done;
}

//////////////////////////////////////////////////
// Swap-out code
//////////////////////////////////////////////////

static struct {
    struct pstate_header *hdr;

    uint64_t disk_page;
    int map_slot;

    struct kobject *ko;
    int extra_page;
} sync_state;

static void
sync_header_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_header_cb: io failure\n");
	pstate_done = -1;
	return;
    }

    //cprintf("sync_header_cb: all done, %ld pages, %d objects\n", sync_state.disk_page, sync_state.map_slot);
    pstate_done = 1;
}

static void sync_kobj();

static void
sync_kobj_cb(disk_io_status stat, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (stat != disk_io_success) {
	cprintf("sync_kobj_cb: disk IO failure\n");
	pstate_done = -1;
	return;
    }

    if (sync_state.ko->ko_extra_pages > sync_state.extra_page) {
	uint64_t offset = sync_state.disk_page * PGSIZE;
	sync_state.disk_page++;

	void *p = kobject_swapout_page(sync_state.ko, sync_state.extra_page);
	sync_state.extra_page++;

	disk_io(op_write, p, PGSIZE, offset, sync_kobj_cb, 0);
    } else {
	sync_state.ko = LIST_NEXT(sync_state.ko, ko_link);
	sync_kobj();
    }
}

static void
sync_kobj()
{
    if (sync_state.ko == 0) {
	sync_state.hdr->ph_map[sync_state.map_slot].offset = 0;
	disk_io(op_write, sync_state.hdr, PGSIZE, 0, sync_header_cb, 0);
    } else {
	if (sync_state.map_slot > NUM_PH_OBJECTS) {
	    cprintf("sync_kobj: too many objects\n");
	    pstate_done = -1;
	    return;
	}

	uint64_t offset = sync_state.disk_page * PGSIZE;
	sync_state.hdr->ph_map[sync_state.map_slot].id = sync_state.ko->ko_id;
	sync_state.hdr->ph_map[sync_state.map_slot].offset = offset;

	sync_state.map_slot++;
	sync_state.disk_page++;
	sync_state.extra_page = 0;

	disk_io(op_write, sync_state.ko, PGSIZE, offset, sync_kobj_cb, 0);
    }
}

void
pstate_sync()
{
    sync_state.hdr = (void *) &pstate_buf[0];
    sync_state.hdr->ph_magic = PSTATE_MAGIC;
    sync_state.hdr->ph_version = PSTATE_VERSION;
    sync_state.hdr->ph_handle_counter = handle_counter;

    sync_state.disk_page = 1;
    sync_state.map_slot = 0;
    sync_state.ko = LIST_FIRST(&ko_list);

    pstate_done = 0;
    sync_kobj();
    while (!pstate_done)
	ide_intr();
}
