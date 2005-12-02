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
#include <inc/setjmp.h>
#include <inc/thread.h>

// Helper functions
static uint64_t syscall_ret;
static struct jmp_buf syscall_retjmp;
static struct kobject *syscall_cleanup_ko;

static void __attribute__((__noreturn__))
syscall_error(int r)
{
    if (syscall_cleanup_ko)
	kobject_free(syscall_cleanup_ko);

    if (r == -E_RESTART)
	thread_syscall_restart(cur_thread);

    syscall_ret = r;
    longjmp(&syscall_retjmp, 1);
}

static int syscall_debug = 0;
#define check(x) _check(x, #x)
static int
_check(int r, const char *what)
{
    if (r < 0) {
	if (syscall_debug)
	    cprintf("syscall check failed: %s\n", what);
	syscall_error(r);
    }

    return r;
}

static int
sysx_thread_jump(struct Thread *t, struct Label *l, struct thread_entry *e)
{
    struct Label *nl;
    int r = label_copy(l, &nl);
    if (r < 0)
	return r;

    thread_jump(t, nl, &e->te_segmap, e->te_entry, e->te_stack, e->te_arg);
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
    syscall_error(-E_RESTART);
}

static int
sys_container_alloc(uint64_t parent_ct)
{
    struct Container *parent;
    check(container_find(&parent, parent_ct));

    struct Container *c;
    check(container_alloc(cur_thread->th_ko.ko_label, &c));
    syscall_cleanup_ko = &c->ct_ko;

    return check(container_put(parent, &c->ct_ko));
}

static void
sys_container_unref(struct cobj_ref cobj)
{
    struct Container *c;
    check(container_find(&c, cobj.container));
    check(container_unref(c, cobj.slot));
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct));
    return check(container_put(c, &cur_thread->th_ko));
}

static int
sys_container_get_type(struct cobj_ref cobj)
{
    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko));
    return ko->ko_type;
}

static int64_t
sys_container_get_c_id(struct cobj_ref cobj)
{
    struct Container *c;
    check(cobj_get(cobj, kobj_container, (struct kobject **)&c));
    return c->ct_ko.ko_id;
}

static int
sys_gate_create(uint64_t container, struct thread_entry *te)
{
    struct Container *c;
    check(container_find(&c, container));

    struct Gate *g;
    check(gate_alloc(cur_thread->th_ko.ko_label, &g));
    syscall_cleanup_ko = &g->gt_ko;

    g->gt_te = *te;
    check(label_copy(cur_thread->th_ko.ko_label, &g->gt_target_label));

    return check(container_put(c, &g->gt_ko));
}

static int
sys_thread_create(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct));

    struct Thread *t;
    check(thread_alloc(cur_thread->th_ko.ko_label, &t));
    syscall_cleanup_ko = &t->th_ko;

    return check(container_put(c, &t->th_ko));
}

static void
sys_gate_enter(struct cobj_ref gt)
{
    struct Gate *g;
    check(cobj_get(gt, kobj_gate, (struct kobject **)&g));
    check(label_compare(cur_thread->th_ko.ko_label, g->gt_ko.ko_label, label_leq_starlo));
    check(sysx_thread_jump(cur_thread, g->gt_target_label, &g->gt_te));
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *s)
{
    struct Thread *t;
    check(cobj_get(thread, kobj_thread, (struct kobject **)&t));

    check(label_compare(t->th_ko.ko_label, cur_thread->th_ko.ko_label, label_eq));

    if (t->th_status != thread_not_started)
	check(-E_INVAL);

    check(sysx_thread_jump(t, cur_thread->th_ko.ko_label, s));
    thread_set_runnable(t);
}

static int
sys_segment_create(uint64_t ct, uint64_t num_pages)
{
    struct Container *c;
    check(container_find(&c, ct));

    struct Segment *sg;
    check(segment_alloc(cur_thread->th_ko.ko_label, &sg));
    syscall_cleanup_ko = &sg->sg_hdr.ko;

    check(segment_set_npages(sg, num_pages));
    return check(container_put(c, &sg->sg_hdr.ko));
}

static void
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_pages)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg));

    check(label_compare(cur_thread->th_ko.ko_label, sg->sg_hdr.ko.ko_label, label_eq));
    check(segment_set_npages(sg, num_pages));
}

static int
sys_segment_get_npages(struct cobj_ref sg_cobj)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg));
    check(label_compare(sg->sg_hdr.ko.ko_label, cur_thread->th_ko.ko_label, label_leq_starhi));
    return sg->sg_hdr.num_pages;
}

static void
sys_segment_get_map(struct segment_map *sm)
{
    *sm = cur_thread->th_segmap;
}

uint64_t
syscall(syscall_num num, uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5)
{
    syscall_cleanup_ko = 0;
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

    case SYS_container_get_type:
	syscall_ret = sys_container_get_type(COBJ(a1, a2));
	break;

    case SYS_container_get_c_id:
	syscall_ret = sys_container_get_c_id(COBJ(a1, a2));
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

    case SYS_segment_create:
	syscall_ret = sys_segment_create(a1, a2);
	break;

    case SYS_segment_resize:
	sys_segment_resize(COBJ(a1, a2), a3);
	break;

    case SYS_segment_get_npages:
	syscall_ret = sys_segment_get_npages(COBJ(a1, a2));
	break;

    case SYS_segment_get_map:
	{
	    struct segment_map s;
	    sys_segment_get_map(&s);

	    page_fault_mode = PFM_KILL;
	    *(struct segment_map *) TRUP(a1) = s;
	    page_fault_mode = PFM_NONE;
	}
	break;

    default:
	cprintf("Unknown syscall %d\n", num);
	syscall_ret = -E_INVAL;
    }

syscall_exit:
    return syscall_ret;
}
