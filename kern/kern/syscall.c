#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/thread.h>
#include <dev/console.h>
#include <dev/kclock.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/lib.h>
#include <kern/container.h>
#include <kern/gate.h>
#include <kern/segment.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <kern/netdev.h>
#include <inc/error.h>
#include <inc/setjmp.h>
#include <inc/thread.h>
#include <inc/netdev.h>

// Helper functions
static uint64_t syscall_ret;
static struct jmp_buf syscall_retjmp;

static void __attribute__((__noreturn__))
syscall_error(int64_t r)
{
    if (r == -E_RESTART)
	thread_syscall_restart(cur_thread);

    syscall_ret = r;
    longjmp(&syscall_retjmp, 1);
}

static int syscall_debug = 0;
#define check(x) _check(x, #x, __LINE__)
static int64_t
_check(int64_t r, const char *what, int line)
{
    if (r < 0) {
	if (syscall_debug)
	    cprintf("syscall check failed (line %d): %s: %s\n",
		    line, what, e2s(r));
	syscall_error(r);
    }

    return r;
}

// Syscall handlers
static void
sys_cons_puts(const char *s, uint64_t size)
{
    int sz = size;
    if (sz < 0)
	syscall_error(-E_INVAL);

    check(page_user_incore((void**) &s, sz));
    cprintf("%.*s", sz, s);
}

static int
sys_cons_getc(void)
{
    int c = cons_getc();
    if (c != 0)
	return c;

    thread_suspend(cur_thread, &console_waiting);
    syscall_error(-E_RESTART);
}

static int64_t
sys_net_wait(uint64_t waiter_id, int64_t waitgen)
{
    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    return check(netdev_thread_wait(ndev, cur_thread, waiter_id, waitgen));
}

static void
sys_net_buf(struct cobj_ref seg, uint64_t offset, netbuf_type type)
{
    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    // XXX think harder about labeling in this case...
    struct Segment *sg;
    check(cobj_get(seg, kobj_segment, (struct kobject **)&sg, iflow_none));
    check(netdev_add_buf(ndev, sg, offset, type));
}

static void
sys_net_macaddr(uint8_t *addrbuf)
{
    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    netdev_macaddr(ndev, addrbuf);
}

static kobject_id_t
sys_container_alloc(uint64_t parent_ct)
{
    struct Container *parent, *c;
    check(container_find(&parent, parent_ct, iflow_write));
    check(container_alloc(&cur_thread->th_ko.ko_label, &c));
    check(container_put(parent, &c->ct_ko));
    return c->ct_ko.ko_id;
}

static void
sys_obj_unref(struct cobj_ref cobj)
{
    // iflow_rw because return code from container_unref
    // indicates the object's presence.
    struct Container *c;
    check(container_find(&c, cobj.container, iflow_rw));

    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    check(container_unref(c, ko));
}

static kobject_id_t
sys_container_get_slot_id(uint64_t ct, uint64_t slot)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_read));

    kobject_id_t id;
    check(container_get(c, &id, slot));
    return id;
}

static uint64_t
sys_handle_create(void)
{
    uint64_t handle = handle_alloc();
    check(label_set(&cur_thread->th_ko.ko_label, handle, LB_LEVEL_STAR));
    return handle;
}

static kobject_type_t
sys_obj_get_type(struct cobj_ref cobj)
{
    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_read));
    return ko->ko_type;
}

static void
sys_obj_get_label(struct cobj_ref cobj, struct ulabel *ul)
{
    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_read));
    check(label_to_ulabel(&ko->ko_label, ul));
}

static void
sys_obj_get_name(struct cobj_ref cobj, char *name)
{
    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_read));
    check(page_user_incore((void **) &name, KOBJ_NAME_LEN));
    strncpy(name, &ko->ko_name[0], KOBJ_NAME_LEN);
}

static void
sys_obj_set_name(struct cobj_ref cobj, char *name)
{
    struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_write));
    check(page_user_incore((void **) &name, KOBJ_NAME_LEN));
    strncpy(&ko->ko_name[0], name, KOBJ_NAME_LEN - 1);
}

static uint64_t
sys_container_nslots(uint64_t container)
{
    struct Container *c;
    check(container_find(&c, container, iflow_read));
    return container_nslots(c);
}

static kobject_id_t
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

    check(container_put(c, &g->gt_ko));
    return g->gt_ko.ko_id;
}

static kobject_id_t
sys_thread_create(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Thread *t;
    check(thread_alloc(&cur_thread->th_ko.ko_label, &t));
    check(container_put(c, &t->th_ko));
    return t->th_ko.ko_id;
}

static void
sys_gate_enter(struct cobj_ref gt, uint64_t a1, uint64_t a2)
{
    struct Gate *g;
    check(cobj_get(gt, kobj_gate, (struct kobject **)&g, iflow_write));

    // XXX do the contaminate, or let the user compute it and verify
    struct thread_entry *e = &g->gt_te;
    thread_jump(cur_thread, &g->gt_target_label,
		e->te_as, e->te_entry, e->te_stack,
		e->te_arg, a1, a2);
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *e)
{
    struct Thread *t;
    check(cobj_get(thread, kobj_thread, (struct kobject **)&t, iflow_rw));

    if (t->th_status != thread_not_started)
	check(-E_INVAL);

    thread_jump(t, &cur_thread->th_ko.ko_label,
		e->te_as, e->te_entry, e->te_stack, e->te_arg, 0, 0);
    thread_set_runnable(t);
}

static void
sys_thread_yield(void)
{
    schedule();
}

static void
sys_thread_halt(void)
{
    thread_halt(cur_thread);
}

static void
sys_thread_sleep(uint64_t msec)
{
    cur_thread->th_wakeup_ticks = timer_ticks + kclock_msec_to_ticks(msec);
    thread_suspend(cur_thread, &timer_sleep);
}

static uint64_t
sys_thread_id(void)
{
    return cur_thread->th_ko.ko_id;
}

static void
sys_thread_addref(uint64_t ct)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));
    check(container_put(c, &cur_thread->th_ko));
}

static void
sys_thread_get_as(struct cobj_ref *as_ref)
{
    *as_ref = cur_thread->th_asref;
}

static kobject_id_t
sys_segment_create(uint64_t ct, uint64_t num_pages)
{
    struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Segment *sg;
    check(segment_alloc(&cur_thread->th_ko.ko_label, &sg));
    check(segment_set_npages(sg, num_pages));
    check(container_put(c, &sg->sg_ko));
    return sg->sg_ko.ko_id;
}

static void
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_pages)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg, iflow_write));
    check(segment_set_npages(sg, num_pages));
}

static uint64_t
sys_segment_get_npages(struct cobj_ref sg_cobj)
{
    struct Segment *sg;
    check(cobj_get(sg_cobj, kobj_segment, (struct kobject **)&sg, iflow_read));
    return sg->sg_ko.ko_npages;
}

static uint64_t
sys_as_create(uint64_t container)
{
    struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Address_space *as;
    check(as_alloc(&cur_thread->th_ko.ko_label, &as));
    check(container_put(c, &as->as_ko));
    return as->as_ko.ko_id;
}

static void
sys_as_get(struct cobj_ref asref, struct u_address_space *uas)
{
    struct Address_space *as;
    check(cobj_get(asref, kobj_address_space, (struct kobject **)&as,
		   iflow_read));
    check(as_to_user(as, uas));
}

static void
sys_as_set(struct cobj_ref asref, struct u_address_space *uas)
{
    struct Address_space *as;
    check(cobj_get(asref, kobj_address_space, (struct kobject **)&as,
		   iflow_write));
    check(as_from_user(as, uas));
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
	sys_cons_puts((const char*) a1, a2);
	break;

    case SYS_cons_getc:
	syscall_ret = sys_cons_getc();
	break;

    case SYS_net_wait:
	syscall_ret = sys_net_wait(a1, a2);
	break;

    case SYS_net_buf:
	sys_net_buf(COBJ(a1, a2), a3, (netbuf_type) a4);
	break;

    case SYS_net_macaddr:
	{
	    uint8_t addrbuf[6];
	    sys_net_macaddr(&addrbuf[0]);

	    check(page_user_incore((void**) &a1, 6));
	    memcpy((void*) a1, &addrbuf[0], 6);
	}
	break;

    case SYS_container_alloc:
	syscall_ret = sys_container_alloc(a1);
	break;

    case SYS_obj_unref:
	sys_obj_unref(COBJ(a1, a2));
	break;

    case SYS_container_get_slot_id:
	syscall_ret = sys_container_get_slot_id(a1, a2);
	break;

    case SYS_handle_create:
	syscall_ret = sys_handle_create();
	break;

    case SYS_obj_get_type:
	syscall_ret = sys_obj_get_type(COBJ(a1, a2));
	break;

    case SYS_obj_get_label:
	sys_obj_get_label(COBJ(a1, a2), (struct ulabel *) a3);
	break;

    case SYS_obj_get_name:
	sys_obj_get_name(COBJ(a1, a2), (char *) a3);
	break;

    case SYS_obj_set_name:
	sys_obj_set_name(COBJ(a1, a2), (char *) a3);
	break;

    case SYS_container_nslots:
	syscall_ret = sys_container_nslots(a1);
	break;

    case SYS_gate_create:
	{
	    struct thread_entry e;
	    check(page_user_incore((void**) &a2, sizeof(e)));
	    memcpy(&e, (void*) a2, sizeof(e));

	    syscall_ret = sys_gate_create(a1, &e,
					  (struct ulabel*) a3,
					  (struct ulabel*) a4);
	}
	break;

    case SYS_gate_enter:
	sys_gate_enter(COBJ(a1, a2), a3, a4);
	break;

    case SYS_thread_create:
	syscall_ret = sys_thread_create(a1);
	break;

    case SYS_thread_start:
	{
	    struct thread_entry e;
	    check(page_user_incore((void**) &a3, sizeof(e)));
	    memcpy(&e, (void*) a3, sizeof(e));

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

    case SYS_thread_id:
	syscall_ret = sys_thread_id();
	break;

    case SYS_thread_addref:
	sys_thread_addref(a1);
	break;

    case SYS_thread_get_as:
	{
	    struct cobj_ref as_ref;
	    sys_thread_get_as(&as_ref);

	    page_user_incore((void**) &a1, sizeof(as_ref));
	    memcpy((void*) a1, &as_ref, sizeof(as_ref));
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

    case SYS_as_create:
	syscall_ret = sys_as_create(a1);
	break;

    case SYS_as_get:
	sys_as_get(COBJ(a1, a2), (struct u_address_space *) a3);
	break;

    case SYS_as_set:
	sys_as_set(COBJ(a1, a2), (struct u_address_space *) a3);
	break;

    default:
	cprintf("Unknown syscall %d\n", num);
	syscall_error(-E_INVAL);
    }

syscall_exit:
    return syscall_ret;
}
