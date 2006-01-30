#include <machine/thread.h>
#include <machine/pmap.h>
#include <machine/as.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <inc/error.h>
#include <inc/cksum.h>

struct kobject_list ko_list;
struct Thread_list kobj_snapshot_waiting;

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

    int r = pstate_swapin(id);
    if (r < 0) {
	// Should match the code returned by container_find() when trying
	// to get an object of the wrong type.
	if (r == -E_NOT_FOUND)
	    r = -E_INVAL;
	return r;
    }

    return -E_RESTART;
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

void
kobject_gc_scan(void)
{
    // Clear cur_thread to avoid putting it to sleep on behalf of
    // our swapped-in objects.
    struct Thread *t = cur_thread;
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

    pagetree_clone(&ko->ko_pt, &snap->u.hdr.ko_pt);
}

void
kobject_snapshot_release(struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    ko->ko_flags &= ~KOBJ_SNAPSHOTING;
    kobject_unpin_hdr(ko);
    pagetree_clone_free(&snap->u.hdr.ko_pt, &ko->ko_pt);

    while (!LIST_EMPTY(&kobj_snapshot_waiting)) {
	struct Thread *t = LIST_FIRST(&kobj_snapshot_waiting);
	thread_set_runnable(t);
    }
}
