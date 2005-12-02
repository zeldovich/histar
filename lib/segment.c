#include <inc/memlayout.h>
#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/thread.h>
#include <inc/assert.h>
#include <inc/error.h>

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
    int npages = sys_segment_get_npages(seg);
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

    // XXX check that va_end < ULIM

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
    int slot, r;
    int newmap = 0;
    struct jmp_buf ret;

    setjmp(&ret);
    if (newmap) {
	sys_container_unref(COBJ(ctemp, slot));
	return 0;
    }

    newmap++;
    struct thread_entry te = {
	.te_segmap = *segmap,
	.te_entry = longjmp,
	.te_stack = 0,
	.te_arg = (uint64_t) &ret,
    };

    slot = sys_gate_create(ctemp, &te);
    if (slot < 0)
	return slot;

    r = sys_gate_enter(COBJ(ctemp, slot));
    if (r < 0)
	return r;

    panic("still alive, sys_gate_enter returned 0");
}
