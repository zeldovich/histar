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
static const struct Label *cur_th_label;
static const struct Label *cur_th_clearance;
static uint64_t syscall_ret;
static struct jos_jmp_buf syscall_retjmp;

static void __attribute__((__noreturn__))
syscall_error(int64_t r)
{
    if (r == -E_RESTART)
	thread_syscall_restart(cur_thread);

    syscall_ret = r;
    jos_longjmp(&syscall_retjmp, 1);
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
    tty_write(s, sz) ;
}

static int64_t
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
    struct Label *cl;
    check(label_alloc(&cl, 1));
    check(label_set(cl, user_root_handle, 0));
    check(label_compare(cur_th_label, cl, label_leq_starlo));

    struct Label *l;
    check(label_alloc(&l, LB_LEVEL_UNDEF));
    check(ulabel_to_label(ul, l));

    struct kobject *ko;
    check(kobject_alloc(kobj_netdev, l, &ko));
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

    check(page_user_incore((void**) &addrbuf, 6));
    netdev_macaddr(ndev, addrbuf);
}

static int64_t
sys_container_alloc(uint64_t parent_ct, struct ulabel *ul, const char *name)
{
    const struct Container *parent;
    check(container_find(&parent, parent_ct, iflow_write));

    const struct Label *l;
    if (ul) {
	struct Label *nl;
	check(label_alloc(&nl, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, nl));
	l = nl;
    } else {
	l = cur_th_label;
    }

    struct Container *c;
    check(container_alloc(l, &c));
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

static int64_t
sys_container_get_slot_id(uint64_t ct, uint64_t slot)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_read));

    kobject_id_t id;
    check(container_get(c, &id, slot));
    return id;
}

static int64_t
sys_handle_create(void)
{
    uint64_t handle = handle_alloc();

    struct Label *l;
    check(label_copy(cur_th_label, &l));
    check(label_set(l, handle, LB_LEVEL_STAR));
    check(thread_change_label(cur_thread, l));

    return handle;
}

static int64_t
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

    const struct Label *l;
    check(kobject_get_label(&ko->hdr, kolabel_contaminate, &l));
    check(label_to_ulabel(l, ul));
}

static void
sys_obj_get_name(struct cobj_ref cobj, char *name)
{
    const struct kobject *ko;
    check(cobj_get(cobj, kobj_any, &ko, iflow_none));
    check(page_user_incore((void **) &name, KOBJ_NAME_LEN));
    strncpy(name, &ko->hdr.ko_name[0], KOBJ_NAME_LEN);
}

static int64_t
sys_obj_get_bytes(struct cobj_ref o)
{
    const struct kobject *ko;
    check(cobj_get(o, kobj_any, &ko, iflow_read));
    return ROUNDUP(KOBJ_DISK_SIZE + ko->hdr.ko_nbytes, 512);
}

static int64_t
sys_container_nslots(uint64_t container)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_read));
    return container_nslots(c);
}

static int64_t
sys_gate_create(uint64_t container, struct thread_entry *ute,
		struct ulabel *ul_recv, struct ulabel *ul_send,
		const char *name)
{
    struct thread_entry te;
    check(page_user_incore((void **) &ute, sizeof(te)));
    memcpy(&te, ute, sizeof(te));

    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    struct Label *l_send;
    check(label_alloc(&l_send, LB_LEVEL_UNDEF));
    check(ulabel_to_label(ul_send, l_send));
    check(label_compare(cur_th_label, l_send, label_leq_starlo));
    check(label_compare(l_send, cur_th_clearance, label_leq_starlo));

    struct Label *clearance_bound;
    check(label_alloc(&clearance_bound, LB_LEVEL_UNDEF));
    check(label_max(cur_th_label, cur_th_clearance,
		    clearance_bound, label_leq_starhi));

    struct Label *clearance;
    check(label_alloc(&clearance, LB_LEVEL_UNDEF));
    check(ulabel_to_label(ul_recv, clearance));
    check(label_compare(clearance, clearance_bound, label_leq_starhi));

    struct Gate *g;
    check(gate_alloc(l_send, clearance, &g));
    alloc_set_name(&g->gt_ko, name);
    g->gt_te = te;

    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &g->gt_ko));
    return g->gt_ko.ko_id;
}

static void
sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul)
{
    const struct kobject *ko;
    check(cobj_get(gate, kobj_gate, &ko, iflow_none));

    const struct Label *clear;
    check(kobject_get_label(&ko->hdr, kolabel_clearance, &clear));
    check(label_to_ulabel(clear, ul));
}

static int64_t
sys_thread_create(uint64_t ct, const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    struct Thread *t;
    check(thread_alloc(cur_th_label, cur_th_clearance, &t));
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

    const struct Label *gt_label, *gt_clearance;
    check(kobject_get_label(&g->gt_ko, kolabel_contaminate, &gt_label));
    check(kobject_get_label(&g->gt_ko, kolabel_clearance, &gt_clearance));

    check(label_compare(cur_th_label, gt_clearance, label_leq_starlo));

    struct Label *label_bound;
    check(label_alloc(&label_bound, LB_LEVEL_UNDEF));
    check(label_max(gt_label, cur_th_label, label_bound, label_leq_starhi));

    struct Label *new_label;
    if (ul == 0) {
	new_label = label_bound;
    } else {
	check(label_alloc(&new_label, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, new_label));
    }
    check(label_compare(label_bound, new_label, label_leq_starlo));

    // Same as the gate clearance except for caller's * handles
    struct Label *clearance_bound;
    check(label_alloc(&clearance_bound, LB_LEVEL_UNDEF));
    check(label_max(gt_clearance, cur_th_label,
		    clearance_bound, label_leq_starhi_rhs_0_except_star));

    const struct Label *new_clearance;
    if (uclearance == 0) {
	new_clearance = gt_clearance;
    } else {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(uclearance, tmp));
	new_clearance = tmp;
    }
    check(label_compare(new_clearance, clearance_bound, label_leq_starhi));

    // Check that we aren't exceeding the clearance in the end
    check(label_compare(new_label, new_clearance, label_leq_starlo));

    const struct thread_entry *e = &g->gt_te;
    check(thread_jump(cur_thread,
		      new_label,
		      new_clearance,
		      e->te_as, e->te_entry, e->te_stack,
		      e->te_arg, 0));
}

static void
sys_thread_start(struct cobj_ref thread, struct thread_entry *ute,
		 struct ulabel *ul, struct ulabel *uclear)
{
    struct thread_entry te;
    check(page_user_incore((void **) &ute, sizeof(te)));
    memcpy(&te, ute, sizeof(te));

    const struct kobject *ko;
    check(cobj_get(thread, kobj_thread, &ko, iflow_rw));

    struct Thread *t = &kobject_dirty(&ko->hdr)->th;
    if (!SAFE_EQUAL(t->th_status, thread_not_started))
	check(-E_INVAL);

    const struct Label *new_label;
    if (ul) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, tmp));
	new_label = tmp;
    } else {
	new_label = cur_th_label;
    }

    const struct Label *new_clearance;
    if (uclear) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(uclear, tmp));
	new_clearance = tmp;
    } else {
	new_clearance = cur_th_clearance;
    }

    check(label_compare(cur_th_label, new_label, label_leq_starlo));
    check(label_compare(new_clearance, cur_th_clearance, label_leq_starhi));

    check(thread_jump(t, new_label, new_clearance,
		      te.te_as, te.te_entry, te.te_stack,
		      te.te_arg, 0));
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

static int64_t
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
    check(page_user_incore((void **) &as_ref, sizeof(*as_ref)));
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
    struct Label *l;
    check(label_alloc(&l, LB_LEVEL_UNDEF));
    check(ulabel_to_label(ul, l));

    check(label_compare(cur_th_label, l, label_leq_starlo));
    check(label_compare(l, cur_th_clearance, label_leq_starlo));
    check(thread_change_label(cur_thread, l));
}

static void
sys_thread_set_clearance(struct ulabel *uclear)
{
    struct Label *clearance;
    check(label_alloc(&clearance, LB_LEVEL_UNDEF));
    check(ulabel_to_label(uclear, clearance));

    struct Label *clearance_bound;
    check(label_alloc(&clearance_bound, LB_LEVEL_UNDEF));
    check(label_max(cur_th_clearance, cur_th_label,
		    clearance_bound, label_leq_starhi));

    check(label_compare(clearance, clearance_bound, label_leq_starhi));
    kobject_set_label_prepared(&kobject_dirty(&cur_thread->th_ko)->hdr,
			       kolabel_clearance, cur_th_clearance, clearance);
}

static void
sys_thread_get_clearance(struct ulabel *uclear)
{
    check(label_to_ulabel(cur_th_clearance, uclear));
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

static int64_t
sys_clock_msec(void)
{
    return timer_user_msec;
}

static int64_t
sys_segment_create(uint64_t ct, uint64_t num_bytes, struct ulabel *ul,
		   const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    const struct Label *l;
    if (ul) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, tmp));
	l = tmp;
    } else {
	l = cur_th_label;
    }

    struct Segment *sg;
    check(segment_alloc(l, &sg));
    alloc_set_name(&sg->sg_ko, name);

    check(segment_set_nbytes(sg, num_bytes));
    check(container_put(&kobject_dirty(&c->ct_ko)->ct, &sg->sg_ko));
    return sg->sg_ko.ko_id;
}

static int64_t
sys_segment_copy(struct cobj_ref seg, uint64_t ct,
		 struct ulabel *ul, const char *name)
{
    const struct Container *c;
    check(container_find(&c, ct, iflow_write));

    const struct kobject *src;
    check(cobj_get(seg, kobj_segment, &src, iflow_read));

    const struct Label *l;
    if (ul) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, tmp));
	l = tmp;
    } else {
	l = cur_th_label;
    }

    struct Segment *sg;
    check(segment_copy(&src->sg, l, &sg));
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

static int64_t
sys_segment_get_nbytes(struct cobj_ref sg_cobj)
{
    const struct kobject *ko;
    check(cobj_get(sg_cobj, kobj_segment, &ko, iflow_read));
    return ko->sg.sg_ko.ko_nbytes;
}

static int64_t
sys_as_create(uint64_t container, struct ulabel *ul, const char *name)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    const struct Label *l;
    if (ul) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, tmp));
	l = tmp;
    } else {
	l = cur_th_label;
    }

    struct Address_space *as;
    check(as_alloc(l, &as));
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

static int64_t
sys_mlt_create(uint64_t container, const char *name)
{
    const struct Container *c;
    check(container_find(&c, container, iflow_write));

    const struct Label *ct_label;
    check(kobject_get_label(&c->ct_ko, kolabel_contaminate, &ct_label));

    struct Mlt *mlt;
    check(mlt_alloc(ct_label, &mlt));
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

    const struct Label *l;
    check(mlt_get(&ko->mt, idx, &l, buf, ct_id));
    if (ul)
	check(label_to_ulabel(l, ul));
}

static void
sys_mlt_put(struct cobj_ref mlt, struct ulabel *ul, uint8_t *buf, kobject_id_t *ct_id)
{
    const struct kobject *ko;
    check(cobj_get(mlt, kobj_mlt, &ko, iflow_read));	// MLT does label check

    const struct Label *l;
    if (ul) {
	struct Label *tmp;
	check(label_alloc(&tmp, LB_LEVEL_UNDEF));
	check(ulabel_to_label(ul, tmp));
	l = tmp;
    } else {
	l = cur_th_label;
    }

    check(label_compare(cur_th_label, l, label_leq_starlo));
    check(page_user_incore((void**) &buf, MLT_BUF_SIZE));
    check(mlt_put(&ko->mt, l, buf, ct_id));
}

typedef void (*void_syscall) ();
typedef int64_t (*s64_syscall) ();
#define SYSCALL_DISPATCH(name) [SYS_##name] = &sys_##name

static void_syscall void_syscalls[NSYSCALLS] = {
    SYSCALL_DISPATCH(cons_puts),
    SYSCALL_DISPATCH(net_macaddr),
    SYSCALL_DISPATCH(net_buf),
    SYSCALL_DISPATCH(obj_unref),
    SYSCALL_DISPATCH(obj_get_label),
    SYSCALL_DISPATCH(obj_get_name),
    SYSCALL_DISPATCH(gate_enter),
    SYSCALL_DISPATCH(gate_clearance),
    SYSCALL_DISPATCH(thread_yield),
    SYSCALL_DISPATCH(thread_start),
    SYSCALL_DISPATCH(thread_halt),
    SYSCALL_DISPATCH(thread_addref),
    SYSCALL_DISPATCH(thread_get_as),
    SYSCALL_DISPATCH(thread_set_as),
    SYSCALL_DISPATCH(thread_set_label),
    SYSCALL_DISPATCH(thread_set_clearance),
    SYSCALL_DISPATCH(thread_get_clearance),
    SYSCALL_DISPATCH(thread_sync_wait),
    SYSCALL_DISPATCH(thread_sync_wakeup),
    SYSCALL_DISPATCH(segment_addref),
    SYSCALL_DISPATCH(segment_resize),
    SYSCALL_DISPATCH(as_get),
    SYSCALL_DISPATCH(as_set),
    SYSCALL_DISPATCH(as_set_slot),
    SYSCALL_DISPATCH(mlt_get),
    SYSCALL_DISPATCH(mlt_put),
};

static s64_syscall s64_syscalls[NSYSCALLS] = {
    SYSCALL_DISPATCH(cons_getc),
    SYSCALL_DISPATCH(net_create),
    SYSCALL_DISPATCH(net_wait),
    SYSCALL_DISPATCH(handle_create),
    SYSCALL_DISPATCH(obj_get_bytes),
    SYSCALL_DISPATCH(obj_get_type),
    SYSCALL_DISPATCH(container_alloc),
    SYSCALL_DISPATCH(container_get_slot_id),
    SYSCALL_DISPATCH(container_nslots),
    SYSCALL_DISPATCH(thread_id),
    SYSCALL_DISPATCH(thread_create),
    SYSCALL_DISPATCH(clock_msec),
    SYSCALL_DISPATCH(segment_create),
    SYSCALL_DISPATCH(segment_copy),
    SYSCALL_DISPATCH(segment_get_nbytes),
    SYSCALL_DISPATCH(gate_create),
    SYSCALL_DISPATCH(as_create),
    SYSCALL_DISPATCH(mlt_create),
};

uint64_t
syscall(syscall_num num, uint64_t a1,
	uint64_t a2, uint64_t a3, uint64_t a4,
	uint64_t a5, uint64_t a6, uint64_t a7)
{
    syscall_ret = 0;

    uint64_t s, f;
    s = read_tsc();

    if (jos_setjmp(&syscall_retjmp) == 0) {
	check(kobject_get_label(&cur_thread->th_ko, kolabel_contaminate,
				&cur_th_label));
	check(kobject_get_label(&cur_thread->th_ko, kolabel_clearance,
				&cur_th_clearance));

	if (num < NSYSCALLS) {
	    void_syscall v_fn = void_syscalls[num];
	    if (v_fn) {
		v_fn(a1, a2, a3, a4, a5, a6, a7);
		goto syscall_exit;
	    }

	    s64_syscall i_fn = s64_syscalls[num];
	    if (i_fn) {
		syscall_ret = i_fn(a1, a2, a3, a4, a5, a6, a7);
		goto syscall_exit;
	    }
	}

	cprintf("Unknown syscall %d\n", num);
	syscall_error(-E_INVAL);
    }

syscall_exit:
    f = read_tsc();
    prof_syscall(num, f - s);

    return syscall_ret;
}
