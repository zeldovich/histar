#include <kern/syscall.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <kern/sched.h>
#include <machine/thread.h>
#include <kern/container.h>
#include <kern/gate.h>
#include <dev/console.h>
#include <kern/segment.h>

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

static void
sys_cputs(const char *s)
{
    page_fault_mode = PFM_KILL;
    cprintf("%s", TRUP(s));
    page_fault_mode = PFM_NONE;
}

static int
sys_cgetc()
{
    int c = cons_getc();
    if (c != 0)
	return c;

    TAILQ_INSERT_TAIL(&console_waiting_tqueue, cur_thread, th_waiting);
    thread_suspend(cur_thread);
    thread_syscall_restart(cur_thread);

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
sys_container_store_cur_pmap(uint64_t ct, int cow_data)
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

    r = container_put(c, cobj_pmap, pgmap);
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
sys_gate_create(uint64_t ct, void *entry, void *stack, uint64_t pm_ctr, uint32_t pm_idx)
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
    g->gt_stack = stack;
    g->gt_arg = 0;
    g->gt_pmap_container = pm_ctr;
    g->gt_pmap_idx = pm_idx;

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

    struct Container *pm_ctr = container_find(g->gt_pmap_container);
    if (pm_ctr == 0)
	return -E_INVAL;

    struct container_object *pm_co = container_get(pm_ctr, g->gt_pmap_idx);
    if (pm_co == 0 || pm_co->type != cobj_pmap)
	return -E_INVAL;

    struct Pagemap *pgmap = pm_co->ptr;
    struct Pagemap *t_pgmap;
    r = page_map_clone(pgmap, &t_pgmap, 0);
    if (r < 0)
	return r;

    // XXX free the cloned page map above in case of failure?
    return thread_jump(t, g->gt_send_label, t_pgmap, g->gt_entry, g->gt_stack, g->gt_arg);
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

static int
sys_pmap_create(uint64_t ct)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    struct Pagemap *pgmap;
    r = page_map_alloc(&pgmap);
    if (r < 0)
	return r;

    r = container_put(c, cobj_pmap, pgmap);
    if (r < 0) {
	// free pgmap
	page_map_addref(pgmap);
	page_map_decref(pgmap);
    }
    return r;
}

static int
sys_segment_create(uint64_t ct, uint64_t num_pages)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(cur_thread->th_label, c->ct_hdr.label, label_eq);
    if (r < 0)
	return r;

    struct Segment *sg;
    r = segment_alloc(&sg);
    if (r < 0)
	return r;

    r = label_copy(cur_thread->th_label, &sg->sg_hdr.label);
    if (r < 0) {
	segment_free(sg);
	return r;
    }

    r = segment_set_npages(sg, num_pages);
    if (r < 0) {
	segment_free(sg);
	return r;
    }

    r = container_put(c, cobj_segment, sg);
    if (r < 0)
	segment_free(sg);
    return r;
}

static int
sys_segment_resize(uint64_t ct, uint64_t idx, uint64_t num_pages)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    struct container_object *co = container_get(c, idx);
    if (co == 0 || co->type != cobj_segment)
	return -E_INVAL;

    struct Segment *sg = co->ptr;
    r = label_compare(cur_thread->th_label, sg->sg_hdr.label, label_eq);
    if (r < 0)
	return r;

    return segment_set_npages(sg, num_pages);
}

static int
sys_segment_get_npages(uint64_t ct, uint64_t idx)
{
    struct Container *c = container_find(ct);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    struct container_object *co = container_get(c, idx);
    if (co == 0 || co->type != cobj_segment)
	return -E_INVAL;

    struct Segment *sg = co->ptr;
    r = label_compare(sg->sg_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    return sg->sg_hdr.num_pages;
}

static int
sys_segment_map(uint64_t sg_ct, uint64_t sg_idx,
		uint64_t pm_ct, uint64_t pm_idx,
		void *va,
		uint64_t start_page, uint64_t num_pages,
		segment_map_mode mode)
{
    struct Container *sg_c = container_find(sg_ct);
    if (sg_c == 0)
	return -E_INVAL;

    int r = label_compare(sg_c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    struct container_object *sg_co = container_get(sg_c, sg_idx);
    if (sg_co == 0 || sg_co->type != cobj_segment)
	return -E_INVAL;

    struct Segment *sg = sg_co->ptr;
    r = label_compare(sg->sg_hdr.label, cur_thread->th_label,
		      (mode == segment_map_rw) ? label_eq : label_leq_starhi);
    if (r < 0)
	return r;

    struct Pagemap *pgmap;
    if (pm_ct == -1 && pm_idx == -1) {
	pgmap = cur_thread->th_pgmap;
    } else {
	struct Container *pm_c = container_find(pm_ct);
	if (pm_c == 0)
	    return -E_INVAL;

	r = label_compare(pm_c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
	if (r < 0)
	    return r;

	struct container_object *pm_co = container_get(pm_c, pm_idx);
	if (pm_co == 0 || pm_co->type != cobj_pmap)
	    return -E_INVAL;

	pgmap = pm_co->ptr;
    }

    // XXX what about pagemap labels?
    return segment_map(pgmap, sg, va, start_page, num_pages, mode);
}

uint64_t
syscall(syscall_num num, uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5)
{
    switch (num) {
    case SYS_yield:
	sys_yield();
	return 0;

    case SYS_halt:
	sys_halt();
	return 0;

    case SYS_cputs:
	sys_cputs((const char*) a1);
	return 0;

    case SYS_cgetc:
	return sys_cgetc((char*) a1);

    case SYS_container_alloc:
	return sys_container_alloc(a1);

    case SYS_container_unref:
	return sys_container_unref(a1, a2);

    case SYS_container_store_cur_thread:
	return sys_container_store_cur_thread(a1);

    case SYS_container_store_cur_pmap:
	return sys_container_store_cur_pmap(a1, a2);

    case SYS_container_get_type:
	return sys_container_get_type(a1, a2);

    case SYS_container_get_c_idx:
	return sys_container_get_c_idx(a1, a2);

    case SYS_gate_create:
	return sys_gate_create(a1, (void*) a2, (void*) a3, a4, a5);

    case SYS_gate_enter:
	return sys_gate_enter(a1, a2);

    case SYS_thread_create:
	return sys_thread_create(a1, a2, a3);

    case SYS_pmap_create:
	return sys_pmap_create(a1);

    case SYS_segment_create:
	return sys_segment_create(a1, a2);

    case SYS_segment_resize:
	return sys_segment_resize(a1, a2, a3);

    case SYS_segment_get_npages:
	return sys_segment_get_npages(a1, a2);

    case SYS_segment_map:
	{
	    page_fault_mode = PFM_KILL;
	    struct segment_map_args *sma = (void*) a1;
	    int r = sys_segment_map(sma->segment.container,
				    sma->segment.idx,
				    sma->pmap.container,
				    sma->pmap.idx,
				    sma->va,
				    sma->start_page,
				    sma->num_pages,
				    sma->mode);
	    page_fault_mode = PFM_NONE;
	    return r;
	}

    default:
	cprintf("Unknown syscall %d\n", num);
	return -E_INVAL;
    }
}
