#include <machine/thread.h>
#include <machine/pmap.h>
#include <machine/as.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <inc/error.h>
#include <inc/cksum.h>

struct kobject_list ko_list;
struct Thread_list kobj_snapshot_waiting;

static int kobject_reclaim_debug = 0;

struct kobject *
kobject_h2k(struct kobject_hdr *kh)
{
    return (struct kobject *) kh;
}

static struct kobject *
kobject_const_h2k(const struct kobject_hdr *kh)
{
    return (struct kobject *) kh;
}

static uint64_t
kobject_cksum(const struct kobject_hdr *ko)
{
    assert(ko);
    uint64_t sum = 0;

    // Compute checksum on everything but ko_cksum + ko_pt
    sum = cksum(sum, ko, offsetof(struct kobject_hdr, ko_cksum));
    sum = cksum(sum, (uint8_t *) ko + sizeof(struct kobject_hdr),
		     sizeof(struct kobject) - sizeof(struct kobject_hdr));

    for (uint64_t i = 0; i < ko->ko_npages; i++) {
	void *p;
	assert(0 == kobject_get_page(ko, i, &p, page_ro));
	assert(p);
	sum = cksum(sum, p, PGSIZE);
    }

    return sum;
}

static int
kobject_iflow_check(struct kobject_hdr *ko, info_flow_type iflow)
{
    int r;

    if (cur_thread == 0)
	return 0;

    switch (iflow) {
    case iflow_read:
	r = label_compare(&ko->ko_label, &cur_thread->th_ko.ko_label,
			  label_leq_starhi);
	break;

    case iflow_write:
	r = label_compare(&cur_thread->th_ko.ko_label, &ko->ko_label,
			  label_leq_starlo);
	break;

    case iflow_rw:
	r = kobject_iflow_check(ko, iflow_read);
	if (r == 0)
	    r = kobject_iflow_check(ko, iflow_write);
	break;

    case iflow_none:
	r = 0;
	break;

    default:
	panic("kobject_get: unknown iflow type %d\n", iflow);
    }

    return r;
}

int
kobject_get(kobject_id_t id, const struct kobject **kp, info_flow_type iflow)
{
    if (id == kobject_id_thread_ct)
	id = cur_thread->th_ct;

    if (id == kobject_id_thread_sg)
	id = cur_thread->th_sg;

    struct kobject_hdr *ko;
    LIST_FOREACH(ko, &ko_list, ko_link) {
	if (ko->ko_id == id) {
	    int r = kobject_iflow_check(ko, iflow);
	    if (r < 0)
		return r;

	    *kp = kobject_h2k(ko);
	    return 0;
	}
    }

    // Should match the code returned by container_find() when trying
    // to get an object of the wrong type.
    if (kobject_negative_contains(id))
	return -E_INVAL;

    return pstate_swapin(id);
}

int
kobject_alloc(kobject_type_t type, const struct Label *l,
	      struct kobject **kp)
{
    int r;

    if (cur_thread) {
	r = label_compare(&cur_thread->th_ko.ko_label, l, label_leq_starlo);
	if (r < 0)
	    return r;
    }

    void *p;
    r = page_alloc(&p);
    if (r < 0)
	return r;

    struct kobject_pair *ko_pair = (struct kobject_pair *) p;
    static_assert(sizeof(*ko_pair) <= PGSIZE);

    struct kobject *ko = &ko_pair->active;
    memset(ko, 0, sizeof(*ko));

    struct kobject_hdr *kh = &ko->u.hdr;
    kh->ko_type = type;
    kh->ko_id = handle_alloc();
    kh->ko_label = *l;
    kh->ko_flags = KOBJ_DIRTY;
    pagetree_init(&kh->ko_pt);

    kobject_negative_remove(kh->ko_id);
    LIST_INSERT_HEAD(&ko_list, kh, ko_link);

    *kp = ko;
    return 0;
}

int
kobject_get_page(const struct kobject_hdr *kp, uint64_t npage, void **pp, page_rw_mode rw)
{
    if (npage >= kp->ko_npages)
	return -E_INVAL;

    if ((kp->ko_flags & KOBJ_SNAPSHOTING) && rw == page_rw && kp->ko_pin_pg) {
	thread_suspend(cur_thread, &kobj_snapshot_waiting);
	return -E_RESTART;
    }

    if (rw == page_rw)
	kobject_dirty(kp);

    int r = pagetree_get_page(&kobject_const_h2k(kp)->u.hdr.ko_pt,
			      npage, pp, rw);
    if (r == 0 && *pp == 0)
	panic("kobject_get_page: id %ld (%s) type %d npage %ld null",
	      kp->ko_id, kp->ko_name, kp->ko_type, npage);
    return r;
}

int
kobject_set_npages(struct kobject_hdr *kp, uint64_t npages)
{
    if (npages > pagetree_maxpages())
	return -E_NO_MEM;

    for (uint64_t i = npages; i < kp->ko_npages; i++) {
	int r = pagetree_put_page(&kp->ko_pt, i, 0);
	if (r < 0) {
	    cprintf("XXX this leaves a hole in the kobject\n");
	    return r;
	}
    }

    for (uint64_t i = kp->ko_npages; i < npages; i++) {
	void *p;
	int r = page_alloc(&p);
	if (r == 0)
	    r = pagetree_put_page(&kp->ko_pt, i, p);

	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (uint64_t j = kp->ko_npages; j < i; j++)
		assert(0 == pagetree_put_page(&kp->ko_pt, j, 0));
	    return r;
	}

	memset(p, 0, PGSIZE);
    }

    kp->ko_npages = npages;
    return 0;
}

struct kobject *
kobject_dirty(const struct kobject_hdr *kh)
{
    struct kobject *ko = kobject_const_h2k(kh);
    ko->u.hdr.ko_flags |= KOBJ_DIRTY;
    return ko;
}

void
kobject_swapin(struct kobject *ko)
{
    uint64_t sum1 = ko->u.hdr.ko_cksum;
    uint64_t sum2 = kobject_cksum(&ko->u.hdr);

    if (sum1 != sum2)
	cprintf("kobject_swapin: %ld checksum mismatch: 0x%lx != 0x%lx\n",
		ko->u.hdr.ko_id, sum1, sum2);

    kobject_negative_remove(ko->u.hdr.ko_id);
    LIST_INSERT_HEAD(&ko_list, &ko->u.hdr, ko_link);
    ko->u.hdr.ko_pin = 0;
    ko->u.hdr.ko_flags &= ~(KOBJ_SNAPSHOTING |
			    KOBJ_DIRTY |
			    KOBJ_SNAPSHOT_DIRTY);

    if (ko->u.hdr.ko_type == kobj_thread)
	thread_swapin(&ko->u.th);
    if (ko->u.hdr.ko_type == kobj_address_space)
	as_swapin(&ko->u.as);
    if (ko->u.hdr.ko_type == kobj_segment)
	segment_swapin(&ko->u.sg);
}

void
kobject_incref(const struct kobject_hdr *ko)
{
    kobject_dirty(ko)->u.hdr.ko_ref++;
}

void
kobject_decref(const struct kobject_hdr *ko)
{
    kobject_dirty(ko)->u.hdr.ko_ref--;
}

void
kobject_pin_hdr(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->u.hdr;
    ++m->ko_pin;
}

void
kobject_unpin_hdr(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->u.hdr;
    --m->ko_pin;
}

void
kobject_pin_page(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->u.hdr;
    ++m->ko_pin_pg;
    ++m->ko_pin;
}

void
kobject_unpin_page(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->u.hdr;
    --m->ko_pin_pg;
    --m->ko_pin;
}

static void
kobject_gc(struct kobject *ko)
{
    int r = 0;

    switch (ko->u.hdr.ko_type) {
    case kobj_thread:
	r = thread_gc(&ko->u.th);
	break;

    case kobj_container:
	r = container_gc(&ko->u.ct);
	break;

    case kobj_address_space:
	r = as_gc(&ko->u.as);
	break;

    case kobj_gate:
    case kobj_segment:
    case kobj_mlt:
    case kobj_netdev:
	break;

    default:
	panic("kobject_free: unknown kobject type %d", ko->u.hdr.ko_type);
    }

    if (r == -E_RESTART)
	return;
    if (r < 0)
	cprintf("kobject_free: cannot GC type %d: %d\n", ko->u.hdr.ko_type, r);

    pagetree_free(&ko->u.hdr.ko_pt);
    ko->u.hdr.ko_npages = 0;
    ko->u.hdr.ko_type = kobj_dead;
}

static void
kobject_gc_scan(void)
{
    // Clear cur_thread to avoid putting it to sleep on behalf of
    // our swapped-in objects.
    const struct Thread *t = cur_thread;
    cur_thread = 0;

    struct kobject_hdr *ko;
    LIST_FOREACH(ko, &ko_list, ko_link)
	if (ko->ko_ref == 0 && ko->ko_pin == 0 && ko->ko_type != kobj_dead)
	    kobject_gc(kobject_dirty(ko));

    cur_thread = t;
}

void
kobject_swapout(struct kobject *ko)
{
    assert(!(ko->u.hdr.ko_flags & KOBJ_SNAPSHOTING));

    if (ko->u.hdr.ko_type == kobj_thread)
	thread_swapout(&ko->u.th);
    if (ko->u.hdr.ko_type == kobj_address_space)
	as_swapout(&ko->u.as);

    LIST_REMOVE(&ko->u.hdr, ko_link);
    pagetree_free(&ko->u.hdr.ko_pt);
    page_free(ko);
}

struct kobject *
kobject_get_snapshot(struct kobject_hdr *ko)
{
    assert((ko->ko_flags & KOBJ_SNAPSHOTING));
    struct kobject_pair *kp = (struct kobject_pair *) ko;
    return &kp->snapshot;
}

void
kobject_snapshot(struct kobject_hdr *ko)
{
    assert(!(ko->ko_flags & KOBJ_SNAPSHOTING));

    if (ko->ko_type == kobj_segment)
	segment_snapshot(&kobject_h2k(ko)->u.sg);

    ko->ko_flags |= KOBJ_SNAPSHOTING;
    ko->ko_flags &= ~KOBJ_DIRTY;
    kobject_pin_hdr(ko);

    uint64_t sum = kobject_cksum(ko);

    struct kobject *snap = kobject_get_snapshot(ko);
    memcpy(snap, ko, sizeof(*snap));
    snap->u.hdr.ko_cksum = sum;

    pagetree_copy(&ko->ko_pt, &snap->u.hdr.ko_pt);
}

void
kobject_snapshot_release(struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    ko->ko_flags &= ~KOBJ_SNAPSHOTING;
    kobject_unpin_hdr(ko);
    pagetree_free(&snap->u.hdr.ko_pt);

    while (!LIST_EMPTY(&kobj_snapshot_waiting)) {
	struct Thread *t = LIST_FIRST(&kobj_snapshot_waiting);
	thread_set_runnable(t);
    }
}

// Negative kobject id cache (objects that don't exist)
enum {
    kobject_neg_nent = 1024
};
static struct {
    int inited;
    int next;
    kobject_id_t ents[kobject_neg_nent];
} kobject_neg;

void
kobject_negative_insert(kobject_id_t id)
{
    if (!kobject_neg.inited)
	for (int i = 0; i < kobject_neg_nent; i++)
	    kobject_neg.ents[i] = kobject_id_null;

    kobject_neg.inited = 1;
    kobject_neg.ents[kobject_neg.next] = id;
    kobject_neg.next = (kobject_neg.next + 1) % kobject_neg_nent;
}

void
kobject_negative_remove(kobject_id_t id)
{
    if (!kobject_neg.inited)
	return;

    for (int i = 0; i < kobject_neg_nent; i++)
	if (kobject_neg.ents[i] == id)
	    kobject_neg.ents[i] = kobject_id_null;
}

bool_t
kobject_negative_contains(kobject_id_t id)
{
    if (!kobject_neg.inited)
	return 0;

    for (int i = 0; i < kobject_neg_nent; i++)
	if (kobject_neg.ents[i] == id)
	    return 1;

    return 0;
}

bool_t
kobject_initial(const struct kobject *ko)
{
    if ((ko->u.hdr.ko_flags & KOBJ_PIN_IDLE))
	return 1;

    if (ko->u.hdr.ko_ref == 0)
	return 1;

    if (ko->u.hdr.ko_type == kobj_thread)
	return ko->u.th.th_status == thread_runnable ||
	       ko->u.th.th_status == thread_suspended;

    return 0;
}

static void
kobject_reclaim(void)
{
    const struct Thread *t = cur_thread;
    cur_thread = 0;

    struct kobject_hdr *next;
    for (struct kobject_hdr *kh = LIST_FIRST(&ko_list); kh != 0; kh = next) {
	next = LIST_NEXT(kh, ko_link);
	struct kobject *ko = kobject_h2k(kh);

	if (kh->ko_pin || (kh->ko_flags & KOBJ_DIRTY) || kobject_initial(ko))
	    continue;

	if (kobject_reclaim_debug)
	    cprintf("kobject_reclaim: swapping out %ld (%s)\n",
		    kh->ko_id, kh->ko_name);

	kobject_swapout(ko);
    }

    cur_thread = t;
}

void
kobject_init(void)
{
    static struct periodic_task gc_pt =
	{ .pt_fn = &kobject_gc_scan, .pt_interval_ticks = 1 };
    timer_add_periodic(&gc_pt);

    static struct periodic_task reclaim_pt =
	{ .pt_fn = &kobject_reclaim };
    reclaim_pt.pt_interval_ticks = kclock_hz;
    timer_add_periodic(&reclaim_pt);
}
