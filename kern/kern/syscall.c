#include <kern/syscall.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <kern/sched.h>
#include <machine/thread.h>
#include <kern/container.h>

static void
sys_cputs(const char *s)
{
    page_fault_mode = PFM_KILL;
    cprintf("%s", TRUP(s));
    page_fault_mode = PFM_NONE;
}

static void
sys_yield()
{
    schedule();
}

static void
sys_halt()
{
    thread_halt(cur_thread);
    schedule();
}

static int
sys_container_alloc(uint64_t parent_ct)
{
    struct Container *parent = container_find(parent_ct);
    if (parent == 0)
	return -E_INVAL;

    // XXX permissions checking
    struct Container *c;
    int r = container_alloc(&c);
    if (r < 0)
	return r;

    r = container_put(parent, cobj_container, c);
    if (r < 0)
	container_free(c);

    return r;
}

static int
sys_container_unref(uint64_t ct, uint32_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    // XXX permissions checking
    container_unref(c, idx);
    return 0;
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    // XXX perm check
    return container_put(c, cobj_thread, cur_thread);
}

static int
sys_container_store_cur_addrspace(uint64_t ct)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    struct Pagemap *pm;
    int r = page_map_clone(cur_thread->th_pgmap, &pm);
    if (r < 0)
	return r;

    r = container_put(c, cobj_address_space, pm);
    if (r < 0)
	page_map_decref(pm);

    return r;
}

static int
sys_container_get_type(uint64_t ct, uint32_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    // XXX perm check
    struct container_object *co = container_get(c, idx);
    if (co == 0)
	return -E_INVAL;

    return co->type;
}

static int64_t
sys_container_get_c_idx(uint64_t ct, uint32_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    // XXX perm check
    struct container_object *co = container_get(c, idx);
    if (co == 0)
	return -E_INVAL;
    if (co->type != cobj_container)
	return -E_INVAL;
    return ((struct Container *) co->ptr)->ct_hdr.idx;
}

uint64_t
syscall(syscall_num num, uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5)
{
    switch (num) {
    case SYS_cputs:
	sys_cputs((const char*) a1);
	return 0;

    case SYS_yield:
	sys_yield();
	return 0;

    case SYS_halt:
	sys_halt();
	return 0;

    case SYS_container_alloc:
	return sys_container_alloc(a1);

    case SYS_container_unref:
	return sys_container_unref(a1, a2);

    case SYS_container_store_cur_thread:
	return sys_container_store_cur_thread(a1);

    case SYS_container_store_cur_addrspace:
	return sys_container_store_cur_addrspace(a1);

    case SYS_container_get_type:
	return sys_container_get_type(a1, a2);

    case SYS_container_get_c_idx:
	return sys_container_get_c_idx(a1, a2);

    default:
	cprintf("Unknown syscall %d\n", num);
	return -E_INVAL;
    }
}
