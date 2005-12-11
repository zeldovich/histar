#include <inc/memlayout.h>
#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/thread.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>

void
segment_map_print(struct segment_map *segmap)
{
    cprintf("segment  start  npages  w  va\n");
    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap->sm_ent[i].num_pages == 0)
	    continue;
	cprintf("%3ld.%-3ld  %5ld  %6ld  %d  %p\n",
		segmap->sm_ent[i].segment.container,
		segmap->sm_ent[i].segment.object,
		segmap->sm_ent[i].start_page,
		segmap->sm_ent[i].num_pages,
		segmap->sm_ent[i].writable,
		segmap->sm_ent[i].va);
    }
}

int
segment_unmap(uint64_t ctemp, void *va)
{
    struct segment_map segmap;
    int r = sys_segment_get_map(&segmap);
    if (r < 0)
	return r;

    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap.sm_ent[i].va == va && segmap.sm_ent[i].num_pages) {
	    segmap.sm_ent[i].num_pages = 0;
	    return segment_map_change(ctemp, &segmap);
	}
    }

    return -E_INVAL;
}

int
segment_map(uint64_t ctemp, struct cobj_ref seg, int writable,
	    void **va_store, uint64_t *bytes_store)
{
    int64_t npages = sys_segment_get_npages(seg);
    if (npages < 0)
	return npages;
    uint64_t bytes = npages * PGSIZE;

    struct segment_map segmap;
    int r = sys_segment_get_map(&segmap);
    if (r < 0)
	return r;

    int free_segslot = -1;
    char *va_start = (char *) 0x100000000;
    char *va_end;

retry:
    va_end = va_start + bytes;
    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap.sm_ent[i].num_pages == 0) {
	    free_segslot = i;
	    continue;
	}

	char *m_start = segmap.sm_ent[i].va;
	char *m_end = m_start + segmap.sm_ent[i].num_pages * PGSIZE;

	if (m_start <= va_end && m_end >= va_start) {
	    va_start = m_end + PGSIZE;
	    goto retry;
	}
    }

    if (va_end >= (char*)ULIM) {
	cprintf("out of virtual address space!\n");
	return -E_NO_MEM;
    }

    if (free_segslot < 0) {
	cprintf("out of segment map slots\n");
	return -E_NO_MEM;
    }

    segmap.sm_ent[free_segslot].segment = seg;
    segmap.sm_ent[free_segslot].start_page = 0;
    segmap.sm_ent[free_segslot].num_pages = npages;
    segmap.sm_ent[free_segslot].writable = writable;
    segmap.sm_ent[free_segslot].va = va_start;

    r = segment_map_change(ctemp, &segmap);
    if (r < 0)
	return r;

    if (bytes_store)
	*bytes_store = bytes;
    if (va_store)
	*va_store = va_start;
    return 0;
}

int
segment_map_change(uint64_t ctemp, struct segment_map *segmap)
{
    //cprintf("segment_map_change:\n");
    //segment_map_print(segmap);

    int64_t gate_id;
    int r;
    int newmap = 0;
    struct jmp_buf ret;

    setjmp(&ret);
    if (newmap) {
	if (gate_id >= 0)
	    sys_obj_unref(COBJ(ctemp, gate_id));
	return 0;
    }

    newmap++;

    uint64_t label_ents[8];
    struct ulabel l = {
	.ul_size = 8,
	.ul_ent = &label_ents[0],
    };

    r = thread_get_label(ctemp, &l);
    if (r < 0)
	return r;

    struct thread_entry te = {
	.te_entry = longjmp,
	.te_stack = 0,
	.te_arg = (uint64_t) &ret,
    };
    memcpy(&te.te_segmap, segmap, sizeof(*segmap));

    gate_id = sys_gate_create(ctemp, &te, &l, &l);
    if (gate_id < 0)
	return gate_id;

    r = sys_gate_enter(COBJ(ctemp, gate_id), 0, 0);
    if (r < 0)
	return r;

    panic("still alive, sys_gate_enter returned 0");
}

int
segment_alloc(uint64_t container, uint64_t bytes, struct cobj_ref *cobj)
{
    uint64_t npages = ROUNDUP(bytes, PGSIZE) / PGSIZE;
    int64_t id = sys_segment_create(container, npages);
    if (id < 0)
	return id;

    *cobj = COBJ(container, id);
    return 0;
}
