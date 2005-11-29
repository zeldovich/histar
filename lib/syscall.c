#include <inc/syscall.h>
#include <inc/syscall_num.h>
#include <inc/types.h>

#include <inc/stdio.h>

void
sys_yield()
{
    syscall(SYS_yield, 0, 0, 0, 0, 0);
}

void
sys_halt()
{
    syscall(SYS_halt, 0, 0, 0, 0, 0);
}

int
sys_cputs(const char *s)
{
    return syscall(SYS_cputs, (uint64_t) s, 0, 0, 0, 0);
}

int
sys_cgetc()
{
    return syscall(SYS_cgetc, 0, 0, 0, 0, 0);
}

int
sys_container_alloc(uint64_t parent)
{
    return syscall(SYS_container_alloc, parent, 0, 0, 0, 0);
}

int
sys_container_unref(struct cobj_ref o)
{
    return syscall(SYS_container_unref, o.container, o.idx, 0, 0, 0);
}

int
sys_container_store_cur_thread(uint64_t container)
{
    return syscall(SYS_container_store_cur_thread, container, 0, 0, 0, 0);
}

int
sys_container_store_cur_addrspace(uint64_t container, int cow_data)
{
    return syscall(SYS_container_store_cur_addrspace, container, cow_data, 0, 0, 0);
}

container_object_type
sys_container_get_type(struct cobj_ref o)
{
    return syscall(SYS_container_get_type, o.container, o.idx, 0, 0, 0);
}

int64_t
sys_container_get_c_idx(struct cobj_ref o)
{
    return syscall(SYS_container_get_c_idx, o.container, o.idx, 0, 0, 0);
}

int
sys_gate_create(uint64_t container, void *entry, void *stack, struct cobj_ref as)
{
    return syscall(SYS_gate_create, container, (uint64_t) entry, (uint64_t) stack, as.container, as.idx);
}

int
sys_gate_enter(struct cobj_ref gate)
{
    return syscall(SYS_gate_enter, gate.container, gate.idx, 0, 0, 0);
}

int
sys_thread_create(uint64_t container, struct cobj_ref gate)
{
    return syscall(SYS_thread_create, container, gate.container, gate.idx, 0, 0);
}

int
sys_segment_create(uint64_t container, uint64_t num_pages)
{
    return syscall(SYS_segment_create, container, num_pages, 0, 0, 0);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_pages)
{
    return syscall(SYS_segment_resize, seg.container, seg.idx, num_pages, 0, 0);
}

int
sys_segment_get_npages(struct cobj_ref seg)
{
    return syscall(SYS_segment_get_npages, seg.container, seg.idx, 0, 0, 0);
}

int
sys_segment_map(struct cobj_ref segment,
		struct cobj_ref as,
		void *va,
		uint64_t start_page,
		uint64_t num_pages,
		segment_map_mode mode)
{
    struct segment_map_args sma;

    sma.segment = segment;
    sma.as = as;
    sma.va = va;
    sma.start_page = start_page;
    sma.num_pages = num_pages;
    sma.mode = mode;

    return syscall(SYS_segment_map, (uint64_t) &sma, 0, 0, 0, 0);
}
