#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/thread.h>
#include <machine/x86.h>
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
#include <kern/kobj.h>
#include <kern/uinit.h>
#include <kern/mlt.h>
#include <kern/sync.h>
#include <kern/prof.h>
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

static void
alloc_set_name(struct kobject_hdr *ko, const char *name)
{
    if (name) {
	check(page_user_incore((void **) &name, KOBJ_NAME_LEN));
	strncpy(&ko->ko_name[0], name, KOBJ_NAME_LEN - 1);
    }
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
sys_net_create(uint64_t container, struct ulabel *ul, const char *name)
{
    // Must have PCL <= { root_handle 0 } to create a netdev
    struct Label cl;
    label_init(&cl, 1);
    check(label_set(&cl, user_root_handle, 0));
    check(label_compare(&cur_thread->th_ko.ko_label, &cl, label_leq_starlo));

    struct Label l;
    check(ulabel_to_label(ul, &l));

    struct kobject *ko;
    check(kobject_alloc(kobj_netdev, &l, &ko));
    alloc_set_name(&ko->hdr, name);

    const struct Container *c;
    check(container_find(&c, container, iflow_write));
    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &ko->hdr));

    return ko->hdr.ko_id;
}

static int64_t
sys_net_wait(struct cobj_ref ndref, uint64_t waiter_id, int64_t waitgen)
{
    const struct kobject *ko;
    check(cobj_get(ndref, kobj_netdev, &ko, iflow_rw));

    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    return check(netdev_thread_wait(ndev, cur_thread, waiter_id, waitgen));
}

static void
sys_net_buf(struct cobj_ref ndref, struct cobj_ref seg, uint64_t offset,
	    netbuf_type type)
{
    const struct kobject *ko;
    check(cobj_get(ndref, kobj_netdev, &ko, iflow_rw));

    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    const struct kobject *ks;
    check(cobj_get(seg, kobj_segment, &ks, iflow_rw));

    const struct Segment *sg = &ks->sg;
    check(netdev_add_buf(ndev, sg, offset, type));
}

static void
sys_net_macaddr(struct cobj_ref ndref, uint8_t *addrbuf)
{
    const struct kobject *ko;
    check(cobj_get(ndref, kobj_netdev, &ko, iflow_read));

    struct net_device *ndev = the_net_device;
    if (ndev == 0)
	syscall_error(-E_INVAL);

    netdev_macaddr(ndev, addrbuf);
}

static kobject_id_t
sys_container_alloc(uint64_t parent_ct, struct ulabel *ul, const char *name)
{
    const struct Container *parent;
    check(container_find(&parent, parent_ct, iflow_write));

    struct Label l;
    if (ul)
	check(ulabel_to_label(ul, &l));
    else
	l = cur_thread->th_ko.ko_label;
    check(label_compare(&cur_thread->th_ko.ko_label, &l, label_leq_starlo));

    struct Container *c;
    check(container_alloc(&l, &c));
    alloc_set_name(&c->ct_ko, name);

    check(container_put(&kobject_dirty(&parent->ct_ko)->ct, &c->ct_ko));
    return c->ct_ko.ko_id;
}

static void
sys_obj_unref(struct cobj_ref cobj)
{
    // iflow_rw because return code from container_unref
    // indicates the object's presence.
    const struct Container *c;
    check(container_find(&c, cobj.container, iflow_rw));

    const struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    check(container_unref(&kobject_dirty(&c->ct_ko)->ct, &ko->hdr));
}

static kobject_id_t
sys_container_get_slot_id(uint64_t ct, uint64_t slot)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_read));

    kobject_id_t id;
    check(container_get(c, &id, slot));
    return id;
}

static uint64_t
sys_handle_create(void)
{
    uint64_t handle = handle_alloc();

    struct Label l = cur_thread->th_ko.ko_label;
    check(label_set(&l, handle, LB_LEVEL_STAR));
    check(thread_change_label(cur_thread, &l));

    return handle;
}

static kobject_type_t
sys_obj_get_type(struct cobj_ref cobj)
{
    const struct kobject *ko;
    // iflow_none because ko_type is immutable
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    return ko->hdr.ko_type;
}

static void
sys_obj_get_label(struct cobj_ref cobj, struct ulabel *ul)
{
    const struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    if ((ko->hdr.ko_flags & KOBJ_LABEL_MUTABLE))
	check(cobj_get(cobj, kobj_any, &ko, iflow_read));
    check(label_to_ulabel(&ko->hdr.ko_label, ul));
}

static void
sys_obj_get_name(struct cobj_ref cobj, char *name)
{
    const struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    check(page_user_incore((void **) &name, KOBJ_NAME_LEN));
    strncpy(name, &ko->hdr.ko_name[0], KOBJ_NAME_LEN);
}

static uint64_t
sys_obj_get_bytes(struct cobj_ref o)
{
    const struct kobject *ko;
    check(cobj_get(o, kobj_any, &ko, iflow_read));
    return ROUNDUP(KOBJ_DISK_SIZE + ko->hdr.ko_nbytes, 512);
}

static uint64_t
sys_container_nslots(uint64_t container)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_read));
    return container_nslots(c);
}

static kobject_id_t
sys_gate_create(uint64_t container, struct thread_entry *te,
		struct ulabel *ul_recv, struct ulabel *ul_send,
		const char *name)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Label l_send;
    check(ulabel_to_label(ul_send, &l_send));

    struct Label clearance_bound;
    check(label_max(&cur_thread->th_ko.ko_label,
		    &cur_thread->th_clearance,
		    &clearance_bound, label_leq_starhi));

    struct Label clearance;
    check(ulabel_to_label(ul_recv, &clearance));
    check(label_compare(&clearance, &clearance_bound, label_leq_starhi));

    struct Gate *g;
    check(gate_alloc(&l_send, &clearance, &g));
    alloc_set_name(&g->gt_ko, name);
    g->gt_te = *te;

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &g->gt_ko));
    return g->gt_ko.ko_id;
}

static void
sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul)
{
    const struct kobject *ko;
    check(cobj_get(gate, kobj_gate, &ko, iflow_none));
    check(label_to_ulabel(&ko->gt.gt_clearance, ul));
}

static kobject_id_t
sys_thread_create(uint64_t ct, const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Thread *t;
    check(thread_alloc(&cur_thread->th_ko.ko_label,
		       &cur_thread->th_clearance,
		       &t));
    alloc_set_name(&t->th_ko, name);

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &t->th_ko));
    return t->th_ko.ko_id;
}

static void
sys_gate_enter(struct cobj_ref gt,
	       struct ulabel *ul,
	       struct ulabel *uclearance)
{
    const struct kobject *ko;
    check(cobj_get(gt, kobj_gate, &ko, iflow_none));

    const struct Gate *g = &ko->gt;
    check(label_compare(&cur_thread->th_ko.ko_label,
			&g->gt_clearance,
			label_leq_starlo));

    struct Label label_bound;
    check(label_max(&g->gt_ko.ko_label, &cur_thread->th_ko.ko_label,
		    &label_bound, label_leq_starhi));

    struct Label new_label;
    if (ul == 0)
	new_label = label_bound;
    else
	check(ulabel_to_label(ul, &new_label));
    check(label_compare(&label_bound, &new_label, label_leq_starlo));

    // Same as the gate clearance except for caller's * handles
    struct Label clearance_bound;
    check(label_max(&g->gt_clearance, &cur_thread->th_ko.ko_label,
		    &clearance_bound, label_leq_starhi_rhs_0_except_star));

    struct Label new_clearance;
    if (uclearance == 0)
	new_clearance = g->gt_clearance;
    else
	check(ulabel_to_label(uclearance, &new_clearance));
    check(label_compare(&new_clearance, &clearance_bound, label_leq_starhi));

    // Check that we aren't exceeding the clearance in the end
    check(label_compare(&new_label, &new_clearance, label_leq_starlo));

    const struct thread_entry *e = &g->gt_te;
    check(thread_jump(cur_thread,
		      &new_label,
		      &new_clearance,
		      e->te_as, e->te_entry, e->te_stack,
		      e->te_arg, 0));
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *e,
		 struct ulabel *ul, struct ulabel *uclear)
{
    const struct kobject *ko;
    check(cobj_get(thread, kobj_thread, &ko, iflow_rw));

    struct Thread *t = &kobject_dirty(&ko->hdr)->th;
    if (!SAFE_EQUAL(t->th_status, thread_not_started))
	check(-E_INVAL);

    struct Label new_label;
    if (ul)
	check(ulabel_to_label(ul, &new_label));
    else
	new_label = cur_thread->th_ko.ko_label;

    struct Label new_clearance;
    if (uclear)
	check(ulabel_to_label(uclear, &new_clearance));
    else
	new_clearance = cur_thread->th_clearance;

    check(label_compare(&cur_thread->th_ko.ko_label, &new_label, label_leq_starlo));
    check(label_compare(&new_clearance, &cur_thread->th_clearance, label_leq_starhi));

    check(thread_jump(t,
		      &new_label, &new_clearance,
		      e->te_as, e->te_entry, e->te_stack,
		      e->te_arg, 0));
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

static uint64_t
sys_thread_id(void)
{
    return cur_thread->th_ko.ko_id;
}

static void
sys_thread_addref(uint64_t ct)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));
    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &cur_thread->th_ko));
}

static void
sys_thread_get_as(struct cobj_ref *as_ref)
{
    *as_ref = cur_thread->th_asref;
}

static void
sys_thread_set_as(struct cobj_ref as_ref)
{
    thread_change_as(cur_thread, as_ref);
}

static void
sys_thread_set_label(struct ulabel *ul)
{
    struct Label l;
    check(ulabel_to_label(ul, &l));

    check(label_compare(&cur_thread->th_ko.ko_label, &l, label_leq_starlo));
    check(label_compare(&l, &cur_thread->th_clearance, label_leq_starlo));
    check(thread_change_label(cur_thread, &l));
}

static void
sys_thread_set_clearance(struct ulabel *uclear)
{
    struct Label clearance;
    check(ulabel_to_label(uclear, &clearance));

    struct Label clearance_bound;
    check(label_max(&cur_thread->th_clearance, &cur_thread->th_ko.ko_label,
		    &clearance_bound, label_leq_starhi));
    check(label_compare(&clearance, &clearance_bound, label_leq_starhi));
    kobject_dirty(&cur_thread->th_ko)->th.th_clearance = clearance;
}

static void
sys_thread_get_clearance(struct ulabel *uclear)
{
    check(label_to_ulabel(&cur_thread->th_clearance, uclear));
}

static void
sys_thread_sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_at_msec)
{
    check(page_user_incore((void**) &addr, sizeof(*addr)));
    check(sync_wait(addr, val, wakeup_at_msec));
}

static void
sys_thread_sync_wakeup(uint64_t *addr)
{
    check(page_user_incore((void**) &addr, sizeof(*addr)));
    sync_wakeup_addr(addr);
}

static uint64_t
sys_clock_msec(void)
{
    return timer_user_msec;
}

static kobject_id_t
sys_segment_create(uint64_t ct, uint64_t num_bytes, struct ulabel *ul,
		   const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Label l;
    if (ul)
	check(ulabel_to_label(ul, &l));
    else
	l = cur_thread->th_ko.ko_label;

    struct Segment *sg;
    check(segment_alloc(&l, &sg));
    alloc_set_name(&sg->sg_ko, name);

    check(segment_set_nbytes(sg, num_bytes));
    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &sg->sg_ko));
    return sg->sg_ko.ko_id;
}

static kobject_id_t
sys_segment_copy(struct cobj_ref seg, uint64_t ct,
		 struct ulabel *ul, const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    const struct kobject *src;
    check(cobj_get(seg, kobj_segment, &src, iflow_read));

    struct Label l;
    if (ul)
	check(ulabel_to_label(ul, &l));
    else
	l = cur_thread->th_ko.ko_label;

    struct Segment *sg;
    check(segment_copy(&src->sg, &l, &sg));
    alloc_set_name(&sg->sg_ko, name);

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &sg->sg_ko));
    return sg->sg_ko.ko_id;
}

static void
sys_segment_addref(struct cobj_ref seg, uint64_t ct)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    const struct kobject *ko;
    check(cobj_get(seg, kobj_segment, &ko, iflow_none));
    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &ko->hdr));
}

static void
sys_segment_resize(struct cobj_ref sg_cobj, uint64_t num_bytes)
{
    const struct kobject *ko;
    check(cobj_get(sg_cobj, kobj_segment, &ko, iflow_write));
    check(segment_set_nbytes(&kobject_dirty(&ko->hdr)->sg, num_bytes));
}

static uint64_t
sys_segment_get_nbytes(struct cobj_ref sg_cobj)
{
    const struct kobject *ko;
    check(cobj_get(sg_cobj, kobj_segment, &ko, iflow_read));
    return ko->sg.sg_ko.ko_nbytes;
}

static uint64_t
sys_as_create(uint64_t container, struct ulabel *ul, const char *name)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Label l;
    if (ul)
	check(ulabel_to_label(ul, &l));
    else
	l = cur_thread->th_ko.ko_label;
    check(label_compare(&cur_thread->th_ko.ko_label, &l, label_leq_starlo));

    struct Address_space *as;
    check(as_alloc(&l, &as));
    alloc_set_name(&as->as_ko, name);

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &as->as_ko));
    return as->as_ko.ko_id;
}

static void
sys_as_get(struct cobj_ref asref, struct u_address_space *uas)
{
    const struct kobject *ko;
    check(cobj_get(asref, kobj_address_space, &ko, iflow_read));
    check(as_to_user(&ko->as, uas));
}

static void
sys_as_set(struct cobj_ref asref, struct u_address_space *uas)
{
    const struct kobject *ko;
    check(cobj_get(asref, kobj_address_space, &ko, iflow_rw));
    check(as_from_user(&kobject_dirty(&ko->hdr)->as, uas));
}

static void
sys_as_set_slot(struct cobj_ref asref, struct u_segment_mapping *usm)
{
    const struct kobject *ko;
    check(cobj_get(asref, kobj_address_space, &ko, iflow_rw));
    check(as_set_uslot(&kobject_dirty(&ko->hdr)->as, usm));
}

static uint64_t
sys_mlt_create(uint64_t container, const char *name)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Mlt *mlt;
    check(mlt_alloc(&c->ct_ko.ko_label, &mlt));
    alloc_set_name(&mlt->mt_ko, name);

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &mlt->mt_ko));
    return mlt->mt_ko.ko_id;
}

static void
sys_mlt_get(struct cobj_ref mlt, uint64_t idx, struct ulabel *ul, uint8_t *buf, kobject_id_t *ct_id)
{
    const struct kobject *ko;
    check(cobj_get(mlt, kobj_mlt, &ko, iflow_read));
    check(page_user_incore((void**) &buf, MLT_BUF_SIZE));
    check(page_user_incore((void**) &ct_id, sizeof(kobject_id_t)));

    struct Label l;
    check(mlt_get(&ko->mt, idx, &l, buf, ct_id));
    if (ul)
	check(label_to_ulabel(&l, ul));
}

static void
sys_mlt_put(struct cobj_ref mlt, struct ulabel *ul, uint8_t *buf, kobject_id_t *ct_id)
{
    const struct kobject *ko;
    check(cobj_get(mlt, kobj_mlt, &ko, iflow_read));	// MLT does label check

    struct Label l;
    if (ul)
	check(ulabel_to_label(ul, &l));
    else
	l = cur_thread->th_ko.ko_label;
    check(label_compare(&cur_thread->th_ko.ko_label, &l, label_leq_starlo));

    check(page_user_incore((void**) &buf, MLT_BUF_SIZE));
    check(mlt_put(&ko->mt, &l, buf, ct_id));
}

uint64_t
syscall(syscall_num num, uint64_t a1,
	uint64_t a2, uint64_t a3, uint64_t a4,
	uint64_t a5, uint64_t a6,
	uint64_t a7 __attribute__((unused)))
{
    syscall_ret = 0;

    uint64_t s, f;
    s = read_tsc();

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

    case SYS_net_create:
	syscall_ret = sys_net_create(a1, (struct ulabel *) a2, (const char *) a3);
	break;

    case SYS_net_wait:
	syscall_ret = sys_net_wait(COBJ(a1, a2), a3, a4);
	break;

    case SYS_net_buf:
	sys_net_buf(COBJ(a1, a2), COBJ(a3, a4), a5, (netbuf_type) a6);
	break;

    case SYS_net_macaddr:
	{
	    uint8_t addrbuf[6];
	    sys_net_macaddr(COBJ(a1, a2), &addrbuf[0]);

	    check(page_user_incore((void**) &a3, 6));
	    memcpy((void*) a3, &addrbuf[0], 6);
	}
	break;

    case SYS_container_alloc:
	syscall_ret = sys_container_alloc(a1, (struct ulabel *) a2, (const char *) a3);
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

    case SYS_obj_get_bytes:
	syscall_ret = sys_obj_get_bytes(COBJ(a1, a2));
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
					  (struct ulabel*) a4,
					  (const char *) a5);
	}
	break;

    case SYS_gate_enter:
	sys_gate_enter(COBJ(a1, a2),
		       (struct ulabel *) a3, (struct ulabel *) a4);
	break;

    case SYS_gate_clearance:
	sys_gate_clearance(COBJ(a1, a2), (struct ulabel *) a3);
	break;

    case SYS_thread_create:
	syscall_ret = sys_thread_create(a1, (const char *) a2);
	break;

    case SYS_thread_start:
	{
	    struct thread_entry e;
	    check(page_user_incore((void**) &a3, sizeof(e)));
	    memcpy(&e, (void*) a3, sizeof(e));

	    sys_thread_start(COBJ(a1, a2), &e,
			     (struct ulabel *) a4,
			     (struct ulabel *) a5);
	}
	break;

    case SYS_thread_yield:
	sys_thread_yield();
	break;

    case SYS_thread_halt:
	sys_thread_halt();
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

	    check(page_user_incore((void**) &a1, sizeof(as_ref)));
	    memcpy((void*) a1, &as_ref, sizeof(as_ref));
	}
	break;

    case SYS_thread_set_as:
	sys_thread_set_as(COBJ(a1, a2));
	break;

    case SYS_thread_set_label:
	sys_thread_set_label((struct ulabel *) a1);
	break;

    case SYS_thread_set_clearance:
	sys_thread_set_clearance((struct ulabel *) a1);
	break;

    case SYS_thread_get_clearance:
	sys_thread_get_clearance((struct ulabel *) a1);
	break;

    case SYS_thread_sync_wait:
	sys_thread_sync_wait((uint64_t *) a1, a2, a3);
	break;

    case SYS_thread_sync_wakeup:
	sys_thread_sync_wakeup((uint64_t *) a1);
	break;

    case SYS_clock_msec:
	syscall_ret = sys_clock_msec();
	break;

    case SYS_segment_create:
	syscall_ret = sys_segment_create(a1, a2, (struct ulabel *) a3,
					 (const char *) a4);
	break;

    case SYS_segment_copy:
	syscall_ret = sys_segment_copy(COBJ(a1, a2), a3,
				       (struct ulabel *) a4,
				       (const char *) a5);
	break;

    case SYS_segment_addref:
	sys_segment_addref(COBJ(a1, a2), a3);
	break;

    case SYS_segment_resize:
	sys_segment_resize(COBJ(a1, a2), a3);
	break;

    case SYS_segment_get_nbytes:
	syscall_ret = sys_segment_get_nbytes(COBJ(a1, a2));
	break;

    case SYS_as_create:
	syscall_ret = sys_as_create(a1, (struct ulabel *) a2, (const char *) a3);
	break;

    case SYS_as_get:
	sys_as_get(COBJ(a1, a2), (struct u_address_space *) a3);
	break;

    case SYS_as_set:
	sys_as_set(COBJ(a1, a2), (struct u_address_space *) a3);
	break;

    case SYS_as_set_slot:
	sys_as_set_slot(COBJ(a1, a2), (struct u_segment_mapping *) a3);
	break;

    case SYS_mlt_create:
	syscall_ret = sys_mlt_create(a1, (const char *) a2);
	break;

    case SYS_mlt_get:
	sys_mlt_get(COBJ(a1, a2), a3, (struct ulabel *) a4,
		    (uint8_t *) a5, (kobject_id_t *) a6);
	break;

    case SYS_mlt_put:
	sys_mlt_put(COBJ(a1, a2), (struct ulabel *) a3,
		    (uint8_t *) a4, (kobject_id_t *) a5);
	break;

    default:
	cprintf("Unknown syscall %d\n", num);
	syscall_error(-E_INVAL);
    }

syscall_exit:
    f = read_tsc();
    prof_syscall(num, f - s);

    return syscall_ret;
}
