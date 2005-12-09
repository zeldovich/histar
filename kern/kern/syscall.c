#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/thread.h>
#include <dev/console.h>
#include <dev/fxp.h>
#include <dev/kclock.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/lib.h>
#include <kern/container.h>
#include <kern/gate.h>
#include <kern/segment.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <inc/error.h>
#include <inc/setjmp.h>
#include <inc/thread.h>
#include <inc/netdev.h>

// Helper functions
static uint64_t syscall_ret;
static struct jmp_buf syscall_retjmp;

static void __attribute__((__noreturn__))
syscall_error(int r)
{
    if (r == -E_RESTART)
	thread_syscall_restart(cur_thread);

    syscall_ret = r;
    longjmp(&syscall_retjmp, 1);
}

static int syscall_debug = 0;
#define check(x) _check(x, #x)
static int64_t
_check(int64_t r, const char *what)
{
    if (r < 0) {
	if (syscall_debug)
	    cprintf("syscall check failed: %s: %s\n", what, e2s(r));
	syscall_error(r);
    }

    return r;
}

// Syscall handlers
static void
sys_cons_puts(const char *s)
{
    page_fault_mode = PFM_KILL;
    cprintf("%s", TRUP(s));
    page_fault_mode = PFM_NONE;
}

static int
sys_cons_getc()
{
    int c = cons_getc();
    if (c != 0)
	return c;

    TAILQ_INSERT_TAIL(&console_waiting_tqueue, cur_thread, th_waiting);
    thread_suspend(cur_thread);
    syscall_error(-E_RESTART);
}

static int64_t
sys_net_wait(uint64_t waiter_id, int64_t waitgen)
{
    return check(fxp_thread_wait(cur_thread, waiter_id, waitgen));
}

static void
sys_net_buf(struct cobj_ref seg, uint64_t offset, netbuf_type type)
{
    // XXX think harder about labeling in this case...
    struct Segment *sg;
    check(cobj_get(seg, kobj_segment, (struct kobject **)&sg, iflow_none));
    check(fxp_add_buf(sg, offset, type));
}

static void
sys_net_macaddr(uint8_t *addrbuf)
{
    fxp_macaddr(addrbuf);
}

static int
sys_container_alloc(uint64_t parent_ct)
{
    struct Container *parent, *c;
    check(container_find(&parent, parent_ct, iflow_write));
    check(container_alloc(&cur_thread->th_ko.ko_label, &c));
    return check(container_put(parent, &c->ct_ko));
}

static void
sys_obj_unref(struct cobj_ref cobj)
{
    struct Container *c;
    check(container_find(&c, cobj.container, iflow_write));
    check(container_unref(c, cobj.slot));
}

static int
sys_container_store_cur_thread(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));
    return check(container_put(c, &cur_thread->th_ko));
}

static uint64_t
sys_handle_create()
{
    uint64_t handle = handle_alloc();
    check(label_set(&cur_thread->th_ko.ko_label, handle, LB_LEVEL_STAR));
    return handle;
}

static int
sys_obj_get_type(struct cobj_ref cobj)
{
    struct kobject *ko;
    // XXX think harder about iflow_none here
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    return ko->ko_type;
}

static int64_t
sys_obj_get_id(struct cobj_ref cobj)
{
    struct kobject *ko;
    // XXX think harder about iflow_none here
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    return ko->ko_id;
}

static void
sys_obj_get_label(struct cobj_ref cobj, struct ulabel *ul)
{
    struct kobject *ko;
    // XXX think harder about iflow_read here
    check(cobj_get(cobj, kobj_any, &ko, iflow_read));
    check(label_to_ulabel(&ko->ko_label, ul));
}

static uint64_t
sys_container_nslots(uint64_t container)
{
    struct Container *c;
    check(container_find(&c, container, iflow_read));
    return container_nslots(c);
}

static int
sys_gate_create(uint64_t container, struct thread_entry *te,
		struct ulabel *ul_e, struct ulabel *ul_t)
{
    struct Label l_e, l_t;
    check(ulabel_to_label(ul_e, &l_e));
    check(ulabel_to_label(ul_t, &l_t));
    check(label_compare(&cur_thread->th_ko.ko_label, &l_t, label_leq_starlo));

    struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Gate *g;
    check(gate_alloc(&l_e, &g));

    g->gt_te = *te;
    g->gt_target_label = l_t;

    return check(container_put(c, &g->gt_ko));
}

static int
sys_thread_create(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Thread *t;
    check(thread_alloc(&cur_thread->th_ko.ko_label, &t));

    return check(container_put(c, &t->th_ko));
}

static void
sys_gate_enter(struct cobj_ref gt)
{
    struct Gate *g;
    check(cobj_get(gt, kobj_gate, (struct kobject **)&g,
		   iflow_write_contaminate));

    // XXX do the contaminate, or let the user compute it and verify
    struct thread_entry *e = &g->gt_te;
    thread_jump(cur_thread, &g->gt_target_label, &e->te_segmap,
		e->te_entry, e->te_stack, e->te_arg);
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *e)
{
    struct Thread *t;
    check(cobj_get(thread, kobj_thread, (struct kobject **)&t, iflow_write));

    if (t->th_status != thread_not_started)
	check(-E_INVAL);

    thread_jump(t, &cur_thread->th_ko.ko_label, &e->te_segmap,
		e->te_entry, e->te_stack, e->te_arg);
    thread_set_runnable(t);
}

static void
sys_thread_yield()
{
    schedule();
}

static void
sys_thread_halt()
{
    thread_halt(cur_thread);
}

static void
sys_thread_sleep(uint64_t msec)
{
    cur_thread->th_wakeup_ticks = timer_ticks + kclock_msec_to_ticks(msec);
    TAILQ_INSERT_TAIL(&timer_sleep_tqueue, cur_thread, th_waiting);
    thread_suspend(cur_thread);
}

static int
sys_segment_create(uint64_t ct, uint64_t num_pages)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Segment *sg;
    check(segment_alloc(&cur_thread->th_ko.ko_label, &sg));

    check(segment_set_npages(sg, num_pages));
    return check(container_put(c, &sg->sg_ko));
}

static void
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_pages)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg, iflow_write));
    check(segment_set_npages(sg, num_pages));
}

static int
sys_segment_get_npages(struct cobj_ref sg_cobj)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg, iflow_read));
    return sg->sg_ko.ko_npages;
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
    syscall_ret = 0;

    int r = setjmp(&syscall_retjmp);
    if (r != 0)
	goto syscall_exit;

    switch (num) {
    case SYS_cons_puts:
	sys_cons_puts((const char*) a1);
	break;

    case SYS_cons_getc:
	syscall_ret = sys_cons_getc((char*) a1);
	break;

    case SYS_net_wait:
	syscall_ret = sys_net_wait(a1, a2);
	break;

    case SYS_net_buf:
	sys_net_buf(COBJ(a1, a2), a3, a4);
	break;

    case SYS_net_macaddr:
	{
	    uint8_t addrbuf[6];
	    sys_net_macaddr(&addrbuf[0]);

	    page_fault_mode = PFM_KILL;
	    memcpy((void*)TRUP(a1), &addrbuf[0], 6);
	    page_fault_mode = PFM_NONE;
	}
	break;

    case SYS_container_alloc:
	syscall_ret = sys_container_alloc(a1);
	break;

    case SYS_obj_unref:
	sys_obj_unref(COBJ(a1, a2));
	break;

    case SYS_container_store_cur_thread:
	syscall_ret = sys_container_store_cur_thread(a1);
	break;

    case SYS_handle_create:
	syscall_ret = sys_handle_create();
	break;

    case SYS_obj_get_type:
	syscall_ret = sys_obj_get_type(COBJ(a1, a2));
	break;

    case SYS_obj_get_id:
	syscall_ret = sys_obj_get_id(COBJ(a1, a2));
	break;

    case SYS_obj_get_label:
	sys_obj_get_label(COBJ(a1, a2), (struct ulabel *) a3);
	break;

    case SYS_container_nslots:
	syscall_ret = sys_container_nslots(a1);
	break;

    case SYS_gate_create:
	{
	    page_fault_mode = PFM_KILL;
	    struct thread_entry e = *(struct thread_entry *) TRUP(a2);
	    page_fault_mode = PFM_NONE;

	    syscall_ret = sys_gate_create(a1, &e,
					  (struct ulabel*) a3,
					  (struct ulabel*) a4);
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

    case SYS_thread_yield:
	sys_thread_yield();
	break;

    case SYS_thread_halt:
	sys_thread_halt();
	break;

    case SYS_thread_sleep:
	sys_thread_sleep(a1);
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
