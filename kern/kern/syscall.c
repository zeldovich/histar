#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/thread.h>
#include <dev/console.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/lib.h>
#include <kern/container.h>
#include <kern/gate.h>
#include <kern/segment.h>
#include <inc/error.h>
#include <inc/syscall_param.h>
#include <inc/setjmp.h>
#include <inc/thread.h>

// Helper functions
static uint64_t syscall_ret;
static struct jmp_buf syscall_retjmp;

static void (*syscall_cleanup) (void *);
static void *syscall_cleanup_arg;
#define SET_SYSCALL_CLEANUP(f, a)		\
    do {					\
	syscall_cleanup = (void(*)(void*)) (f);	\
	syscall_cleanup_arg = (a);		\
    } while (0)

typedef enum {
    lookup_read,
    lookup_modify
} lookup_type;

static void __attribute__((__noreturn__))
sysx_error(int r)
{
    if (syscall_cleanup)
	syscall_cleanup(syscall_cleanup_arg);

    if (r == -E_RESTART)
	thread_syscall_restart(cur_thread);

    syscall_ret = r;
    longjmp(&syscall_retjmp, 1);
}

static int
check(int r)
{
    if (r < 0)
	sysx_error(r);
    return r;
}

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

    struct container_object *co = container_get(c, cobj.slot);
    if (co == 0 || (cotype != cobj_any && co->type != cotype))
	return -E_INVAL;

    *cp = co;
    return 0;
}

static int
sysx_get_pmap(struct Pagemap **pmapp, struct cobj_ref cobj, struct Thread *t)
{
    if (cobj.container == -1 && cobj.slot == -1) {
	*pmapp = t->th_pgmap;
	return 0;
    } else {
	struct container_object *co;
	int r = sysx_get_cobj(&co, cobj, cobj_pmap, t);
	if (r < 0)
	    return r;

	*pmapp = co->ptr;
	return 0;
    }
}

static int
sysx_thread_jump(struct Thread *t, struct Label *l, struct thread_entry *e)
{
    struct container_object *co;
    int r = sysx_get_cobj(&co, e->te_pmap, cobj_pmap, t);
    if (r < 0)
	return r;

    struct Pagemap *g_pgmap = co->ptr;
    struct Pagemap *t_pgmap;
    if (e->te_pmap_copy) {
	r = page_map_clone(g_pgmap, &t_pgmap, 0);
	if (r < 0)
	    return r;
    } else {
	t_pgmap = g_pgmap;
    }

    struct Label *nl;
    r = label_copy(l, &nl);
    if (r < 0)
	return r;

    thread_jump(t, nl, t_pgmap, e->te_entry, e->te_stack, e->te_arg);
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
    sysx_error(-E_RESTART);
}

static int
sys_container_alloc(uint64_t parent_ct)
{
    struct Container *parent;
    check(sysx_get_container(&parent, parent_ct, cur_thread, lookup_modify));

    struct Container *c;
    check(container_alloc(&c));
    SET_SYSCALL_CLEANUP(container_free, c);

    check(label_copy(cur_thread->th_label, &c->ct_hdr.label));
    return check(container_put(parent, cobj_container, c));
}

static void
sys_container_unref(struct cobj_ref cobj)
{
    struct Container *c;
    check(sysx_get_container(&c, cobj.container, cur_thread, lookup_modify));

    container_unref(c, cobj.slot);
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c;
    check(sysx_get_container(&c, ct, cur_thread, lookup_modify));

    return container_put(c, cobj_thread, cur_thread);
}

static int
sys_container_store_cur_pmap(uint64_t ct, int copy)
{
    struct Container *c;
    check(sysx_get_container(&c, ct, cur_thread, lookup_modify));

    struct Pagemap *pgmap = cur_thread->th_pgmap;
    if (copy) {
	check(page_map_clone(pgmap, &pgmap, 1));
	SET_SYSCALL_CLEANUP(page_map_free, pgmap);
    }

    return check(container_put(c, cobj_pmap, pgmap));
}

static int
sys_container_get_type(struct cobj_ref cobj)
{
    struct container_object *co;
    check(sysx_get_cobj(&co, cobj, cobj_any, cur_thread));

    return co->type;
}

static int64_t
sys_container_get_c_idx(struct cobj_ref cobj)
{
    struct container_object *co;
    check(sysx_get_cobj(&co, cobj, cobj_container, cur_thread));

    return ((struct Container *) co->ptr)->ct_hdr.idx;
}

static int
sys_gate_create(uint64_t container, struct thread_entry *te)
{
    struct Container *c;
    check(sysx_get_container(&c, container, cur_thread, lookup_modify));

    struct Gate *g;
    check(gate_alloc(&g));
    SET_SYSCALL_CLEANUP(gate_free, g);

    g->gt_te = *te;

    check(label_copy(cur_thread->th_label, &g->gt_recv_label));
    check(label_copy(cur_thread->th_label, &g->gt_send_label));
    return check(container_put(c, cobj_gate, g));
}

static int
sys_thread_create(uint64_t ct)
{
    struct Container *c;
    check(sysx_get_container(&c, ct, cur_thread, lookup_modify));

    struct Thread *t;
    check(thread_alloc(&t));
    SET_SYSCALL_CLEANUP(thread_free, t);

    check(label_copy(cur_thread->th_label, &t->th_label));
    int tslot = check(container_put(c, cobj_thread, t));

    return tslot;
}

static void
sys_gate_enter(struct cobj_ref gt)
{
    struct container_object *co_gt;
    check(sysx_get_cobj(&co_gt, gt, cobj_gate, cur_thread));

    struct Gate *g = co_gt->ptr;
    check(label_compare(cur_thread->th_label, g->gt_recv_label, label_leq_starlo));

    check(sysx_thread_jump(cur_thread, g->gt_send_label, &g->gt_te));
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *s)
{
    struct container_object *t_co;
    check(sysx_get_cobj(&t_co, thread, cobj_thread, cur_thread));

    struct Thread *t = t_co->ptr;
    check(label_compare(t->th_label, cur_thread->th_label, label_eq));

    if (t->th_status != thread_not_started)
	check(-E_INVAL);

    check(sysx_thread_jump(t, cur_thread->th_label, s));
    thread_set_runnable(t);
}

static int
sys_pmap_create(uint64_t ct)
{
    struct Container *c;
    check(sysx_get_container(&c, ct, cur_thread, lookup_modify));

    struct Pagemap *pgmap;
    check(page_map_alloc(&pgmap));
    SET_SYSCALL_CLEANUP(page_map_free, pgmap);

    return check(container_put(c, cobj_pmap, pgmap));
}

static void
sys_pmap_unmap(struct cobj_ref pmap, void *va, uint64_t num_pages)
{
    struct Pagemap *pgmap;
    check(sysx_get_pmap(&pgmap, pmap, cur_thread));

    char *cva = (char *) va;
    for (int i = 0; i < num_pages; i++) {
	if ((uint64_t)cva >= ULIM)
	    check(-E_INVAL);

	page_remove(pgmap, cva);
	cva += PGSIZE;
    }
}

static int
sys_segment_create(uint64_t ct, uint64_t num_pages)
{
    struct Container *c;
    check(sysx_get_container(&c, ct, cur_thread, lookup_modify));

    struct Segment *sg;
    check(segment_alloc(&sg));
    SET_SYSCALL_CLEANUP(segment_free, sg);

    check(label_copy(cur_thread->th_label, &sg->sg_hdr.label));
    check(segment_set_npages(sg, num_pages));
    return check(container_put(c, cobj_segment, sg));
}

static void
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_pages)
{
    struct container_object *co;
    check(sysx_get_cobj(&co, sg_cobj, cobj_segment, cur_thread));

    struct Segment *sg = co->ptr;
    check(label_compare(cur_thread->th_label, sg->sg_hdr.label, label_eq));
    check(segment_set_npages(sg, num_pages));
}

static int
sys_segment_get_npages(struct cobj_ref sg_cobj)
{
    struct container_object *co;
    check(sysx_get_cobj(&co, sg_cobj, cobj_segment, cur_thread));

    struct Segment *sg = co->ptr;
    check(label_compare(sg->sg_hdr.label, cur_thread->th_label, label_leq_starhi));

    return sg->sg_hdr.num_pages;
}

static void
sys_segment_map(struct sys_segment_map_args *a)
{
    struct container_object *co_sg;
    check(sysx_get_cobj(&co_sg, a->segment, cobj_segment, cur_thread));

    struct Segment *sg = co_sg->ptr;
    check(label_compare(sg->sg_hdr.label, cur_thread->th_label,
		        (a->mode == segment_map_rw) ? label_eq : label_leq_starhi));

    struct Pagemap *pgmap;
    check(sysx_get_pmap(&pgmap, a->pmap, cur_thread));

    // XXX what about pagemap labels?
    check(segment_map(pgmap, sg, a->va, a->start_page, a->num_pages, a->mode));
}

uint64_t
syscall(syscall_num num, uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5)
{
    syscall_cleanup = 0;
    syscall_ret = 0;

    int r = setjmp(&syscall_retjmp);
    if (r != 0)
	goto syscall_exit;

    switch (num) {
    case SYS_yield:
	sys_yield();
	break;

    case SYS_halt:
	sys_halt();
	break;

    case SYS_cputs:
	sys_cputs((const char*) a1);
	break;

    case SYS_cgetc:
	syscall_ret = sys_cgetc((char*) a1);
	break;

    case SYS_container_alloc:
	syscall_ret = sys_container_alloc(a1);
	break;

    case SYS_container_unref:
	sys_container_unref(COBJ(a1, a2));
	break;

    case SYS_container_store_cur_thread:
	syscall_ret = sys_container_store_cur_thread(a1);
	break;

    case SYS_container_store_cur_pmap:
	syscall_ret = sys_container_store_cur_pmap(a1, a2);
	break;

    case SYS_container_get_type:
	syscall_ret = sys_container_get_type(COBJ(a1, a2));
	break;

    case SYS_container_get_c_idx:
	syscall_ret = sys_container_get_c_idx(COBJ(a1, a2));
	break;

    case SYS_gate_create:
	{
	    page_fault_mode = PFM_KILL;
	    struct thread_entry e = *(struct thread_entry *) TRUP(a2);
	    page_fault_mode = PFM_NONE;

	    syscall_ret = sys_gate_create(a1, &e);
	}
	break;

    case SYS_gate_enter:
	sys_gate_enter(COBJ(a1, a2));
	break;

    case SYS_thread_create:
	syscall_ret = sys_thread_create(a1);
	break;

    case SYS_thread_start:
	{
	    page_fault_mode = PFM_KILL;
	    struct thread_entry e = *(struct thread_entry *) TRUP(a3);
	    page_fault_mode = PFM_NONE;

	    sys_thread_start(COBJ(a1, a2), &e);
	}
	break;

    case SYS_pmap_create:
	syscall_ret = sys_pmap_create(a1);
	break;

    case SYS_pmap_unmap:
	sys_pmap_unmap(COBJ(a1, a2), (void*) a3, a4);
	break;

    case SYS_segment_create:
	syscall_ret = sys_segment_create(a1, a2);
	break;

    case SYS_segment_resize:
	sys_segment_resize(COBJ(a1, a2), a3);
	break;

    case SYS_segment_get_npages:
	syscall_ret = sys_segment_get_npages(COBJ(a1, a2));
	break;

    case SYS_segment_map:
	{
	    page_fault_mode = PFM_KILL;
	    struct sys_segment_map_args a =
		*(struct sys_segment_map_args *) TRUP(a1);
	    page_fault_mode = PFM_NONE;

	    sys_segment_map(&a);
	}
	break;

    default:
	cprintf("Unknown syscall %d\n", num);
	syscall_ret = -E_INVAL;
    }

syscall_exit:
    return syscall_ret;
}
