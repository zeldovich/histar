#include <kern/syscall.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <kern/sched.h>
#include <machine/thread.h>
#include <kern/container.h>
#include <kern/gate.h>

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

    int r = label_compare(cur_thread->th_label, parent->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    struct Container *c;
    r = container_alloc(&c);
    if (r < 0)
	return r;

    r = label_copy(cur_thread->th_label, &c->ct_hdr.label);
    if (r < 0) {
	container_free(c);
	return r;
    }

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

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    container_unref(c, idx);
    return 0;
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    return container_put(c, cobj_thread, cur_thread);
}

static int
sys_container_store_cur_addrspace(uint64_t ct, int cow_data)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    struct Pagemap *pgmap;
    r = page_map_clone(cur_thread->th_pgmap, &pgmap, cow_data);
    if (r < 0)
	return r;

    r = container_put(c, cobj_address_space, pgmap);
    if (r < 0) {
	// free pgmap
	page_map_addref(pgmap);
	page_map_decref(pgmap);
    }

    return r;
}

static int
sys_container_get_type(uint64_t ct, uint32_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

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

    int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    struct container_object *co = container_get(c, idx);
    if (co == 0 || co->type != cobj_container)
	return -E_INVAL;
    return ((struct Container *) co->ptr)->ct_hdr.idx;
}

static int
sys_gate_create(uint64_t ct, void *entry, uint64_t arg, uint64_t as_ctr, uint32_t as_idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_eq);
    if (r < 0)
	return r;

    struct Gate *g;
    r = gate_alloc(&g);
    if (r < 0)
	return r;

    g->gt_entry = entry;
    g->gt_arg = arg;
    g->gt_as_container = as_ctr;
    g->gt_as_idx = as_idx;

    r = label_copy(cur_thread->th_label, &g->gt_recv_label);
    if (r < 0) {
	gate_free(g);
	return r;
    }

    r = label_copy(cur_thread->th_label, &g->gt_send_label);
    if (r < 0) {
	gate_free(g);
	return r;
    }

    r = container_put(c, cobj_gate, g);
    if (r < 0)
	gate_free(g);

    return r;
}

static int
thread_gate_enter(struct Thread *t, uint64_t ct, uint64_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, t->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    struct container_object *co = container_get(c, idx);
    if (co == 0 || co->type != cobj_gate)
	return -E_INVAL;

    struct Gate *g = co->ptr;
    r = label_compare(t->th_label, g->gt_recv_label, label_leq_starlo);
    if (r < 0)
	return r;

    struct Container *as_ctr = container_find(g->gt_as_container);
    if (as_ctr == 0)
	return -E_INVAL;

    struct container_object *as_co = container_get(as_ctr, g->gt_as_idx);
    if (as_co == 0 || as_co->type != cobj_address_space)
	return -E_INVAL;

    struct Pagemap *as_pgmap = as_co->ptr;
    struct Pagemap *t_pgmap;
    r = page_map_clone(as_pgmap, &t_pgmap, 0);
    if (r < 0)
	return r;

    // XXX free the cloned page map above in case of failure?
    return thread_jump(t, g->gt_send_label, t_pgmap, g->gt_entry, g->gt_arg);
}

static int
sys_gate_enter(uint64_t ct, uint64_t idx)
{
    return thread_gate_enter(cur_thread, ct, idx);
}

static int
sys_thread_create(uint64_t ct, uint64_t gt_ctr, uint32_t gt_idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    struct Thread *t;
    r = thread_alloc(&t);
    if (r < 0)
	return r;

    r = label_copy(cur_thread->th_label, &t->th_label);
    if (r < 0) {
	thread_free(t);
	return r;
    }

    int tidx = container_put(c, cobj_thread, t);
    if (tidx < 0) {
	thread_free(t);
	return tidx;
    }

    r = thread_gate_enter(t, gt_ctr, gt_idx);
    if (r < 0) {
	container_unref(c, tidx);
	return r;
    }

    thread_set_runnable(t);
    return tidx;
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
	return sys_container_store_cur_addrspace(a1, a2);

    case SYS_container_get_type:
	return sys_container_get_type(a1, a2);

    case SYS_container_get_c_idx:
	return sys_container_get_c_idx(a1, a2);

    case SYS_gate_create:
	return sys_gate_create(a1, (void*) a2, a3, a4, a5);

    case SYS_gate_enter:
	return sys_gate_enter(a1, a2);

    case SYS_thread_create:
	return sys_thread_create(a1, a2, a3);

    default:
	cprintf("Unknown syscall %d\n", num);
	return -E_INVAL;
    }
}
