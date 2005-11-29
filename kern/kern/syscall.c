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

// Helper functions
typedef enum {
    lookup_read,
    lookup_modify
} lookup_type;

static int
sysx_get_container(struct Container **cp, uint64_t cidx, struct Thread *t, lookup_type lt)
{
    struct Container *c = container_find(cidx);
    if (c == 0)
	return -E_INVAL;

    int r = label_compare(c->ct_hdr.label, t->th_label,
			  lt == lookup_modify ? label_eq : label_leq_starhi);
    if (r < 0)
	return r;

    *cp = c;
    return 0;
}

static int
sysx_get_cobj(struct container_object **cp, struct cobj_ref cobj, container_object_type cotype, struct Thread *t)
{
    struct Container *c;
    int r = sysx_get_container(&c, cobj.container, t, lookup_read);
    if (r < 0)
	return r;

    struct container_object *co = container_get(c, cobj.idx);
    if (co == 0 || (cotype != cobj_any && co->type != cotype))
	return -E_INVAL;

    *cp = co;
    return 0;
}

// Syscall handlers
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
    struct Container *parent;
    int r = sysx_get_container(&parent, parent_ct, cur_thread, lookup_modify);
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
sys_container_unref(struct cobj_ref cobj)
{
    struct Container *c;
    int r = sysx_get_container(&c, cobj.container, cur_thread, lookup_modify);
    if (r < 0)
	return r;

    container_unref(c, cobj.idx);
    return 0;
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
    if (r < 0)
	return r;

    return container_put(c, cobj_thread, cur_thread);
}

static int
sys_container_store_cur_pmap(uint64_t ct, int cow_data)
{
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
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
sys_container_get_type(struct cobj_ref cobj)
{
    struct container_object *co;
    int r = sysx_get_cobj(&co, cobj, cobj_any, cur_thread);
    if (r < 0)
	return r;

    return co->type;
}

static int64_t
sys_container_get_c_idx(struct cobj_ref cobj)
{
    struct container_object *co;
    int r = sysx_get_cobj(&co, cobj, cobj_container, cur_thread);
    if (r < 0)
	return r;

    return ((struct Container *) co->ptr)->ct_hdr.idx;
}

static int
sys_gate_create(uint64_t ct, void *entry, void *stack, struct cobj_ref pm_cobj)
{
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
    if (r < 0)
	return r;

    struct Gate *g;
    r = gate_alloc(&g);
    if (r < 0)
	return r;

    g->gt_entry = entry;
    g->gt_stack = stack;
    g->gt_arg = 0;
    g->gt_pmap_cobj = pm_cobj;

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
thread_gate_enter(struct Thread *t, struct cobj_ref gt_cobj)
{
    struct container_object *co_gt;
    int r = sysx_get_cobj(&co_gt, gt_cobj, cobj_gate, t);
    if (r < 0)
	return r;

    struct Gate *g = co_gt->ptr;
    r = label_compare(t->th_label, g->gt_recv_label, label_leq_starlo);
    if (r < 0)
	return r;

    // XXX
    // need a more sensible label check here;
    // probably need to look up the target address space with
    // the send label of the gate.
    struct container_object *co_pm;
    r = sysx_get_cobj(&co_pm, g->gt_pmap_cobj, cobj_pmap, t);
    if (r < 0)
	return r;

    struct Pagemap *t_pgmap;
    r = page_map_clone(co_pm->ptr, &t_pgmap, 0);
    if (r < 0)
	return r;

    // XXX free the cloned page map above in case of failure?
    return thread_jump(t, g->gt_send_label, t_pgmap, g->gt_entry, g->gt_stack, g->gt_arg);
}

static int
sys_gate_enter(struct cobj_ref gt)
{
    return thread_gate_enter(cur_thread, gt);
}

static int
sys_thread_create(uint64_t ct, struct cobj_ref gt)
{
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
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

    r = thread_gate_enter(t, gt);
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
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
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
    struct Container *c;
    int r = sysx_get_container(&c, ct, cur_thread, lookup_modify);
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
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_pages)
{
    struct container_object *co;
    int r = sysx_get_cobj(&co, sg_cobj, cobj_segment, cur_thread);
    if (r < 0)
	return r;

    struct Segment *sg = co->ptr;
    r = label_compare(cur_thread->th_label, sg->sg_hdr.label, label_eq);
    if (r < 0)
	return r;

    return segment_set_npages(sg, num_pages);
}

static int
sys_segment_get_npages(struct cobj_ref sg_cobj)
{
    struct container_object *co;
    int r = sysx_get_cobj(&co, sg_cobj, cobj_segment, cur_thread);
    if (r < 0)
	return r;

    struct Segment *sg = co->ptr;
    r = label_compare(sg->sg_hdr.label, cur_thread->th_label, label_leq_starhi);
    if (r < 0)
	return r;

    return sg->sg_hdr.num_pages;
}

static int
sys_segment_map(struct cobj_ref sg_cobj,
		struct cobj_ref pm_cobj,
		void *va,
		uint64_t start_page, uint64_t num_pages,
		segment_map_mode mode)
{
    struct container_object *co_sg;
    int r = sysx_get_cobj(&co_sg, sg_cobj, cobj_segment, cur_thread);
    if (r < 0)
	return r;

    struct Segment *sg = co_sg->ptr;
    r = label_compare(sg->sg_hdr.label, cur_thread->th_label,
		      (mode == segment_map_rw) ? label_eq : label_leq_starhi);
    if (r < 0)
	return r;

    struct Pagemap *pgmap;
    if (pm_cobj.container == -1 && pm_cobj.idx == -1) {
	pgmap = cur_thread->th_pgmap;
    } else {
	struct container_object *co_pm;
	r = sysx_get_cobj(&co_pm, pm_cobj, cobj_pmap, cur_thread);
	if (r < 0)
	    return r;
	pgmap = co_pm->ptr;
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
	return sys_container_unref(COBJ(a1, a2));

    case SYS_container_store_cur_thread:
	return sys_container_store_cur_thread(a1);

    case SYS_container_store_cur_pmap:
	return sys_container_store_cur_pmap(a1, a2);

    case SYS_container_get_type:
	return sys_container_get_type(COBJ(a1, a2));

    case SYS_container_get_c_idx:
	return sys_container_get_c_idx(COBJ(a1, a2));

    case SYS_gate_create:
	return sys_gate_create(a1, (void*) a2, (void*) a3, COBJ(a4, a5));

    case SYS_gate_enter:
	return sys_gate_enter(COBJ(a1, a2));

    case SYS_thread_create:
	return sys_thread_create(a1, COBJ(a2, a3));

    case SYS_pmap_create:
	return sys_pmap_create(a1);

    case SYS_segment_create:
	return sys_segment_create(a1, a2);

    case SYS_segment_resize:
	return sys_segment_resize(COBJ(a1, a2), a3);

    case SYS_segment_get_npages:
	return sys_segment_get_npages(COBJ(a1, a2));

    case SYS_segment_map:
	{
	    page_fault_mode = PFM_KILL;
	    struct segment_map_args *sma = (void*) TRUP(a1);
	    int r = sys_segment_map(sma->segment,
				    sma->pmap,
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
