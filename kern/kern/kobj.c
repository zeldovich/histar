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

struct kobject_list ko_list;
struct Thread_list kobj_snapshot_waiting;

static int
kobject_iflow_check(struct kobject *ko, info_flow_type iflow)
{
    int r;

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
kobject_get(kobject_id_t id, struct kobject **kp, info_flow_type iflow)
{
    LIST_FOREACH(*kp, &ko_list, ko_link) {
	if ((*kp)->ko_id == id) {
	    int r = kobject_iflow_check(*kp, iflow);
	    if (r < 0)
		return r;

	    if (iflow == iflow_rw || iflow == iflow_write)
		(*kp)->ko_flags |= KOBJ_DIRTY;
	    return 0;
	}
    }

    int r = pstate_swapin(id);
    if (r < 0)
	return r;

    return -E_RESTART;
}

int
kobject_alloc(kobject_type_t type, struct Label *l, struct kobject **kp)
{
    void *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct kobject_pair *ko_pair = p;
    static_assert(sizeof(*ko_pair) <= PGSIZE);

    struct kobject *ko = &ko_pair->active.u.hdr;
    memset(ko, 0, sizeof(*ko));
    ko->ko_type = type;
    ko->ko_id = handle_alloc();
    ko->ko_label = *l;
    ko->ko_flags = KOBJ_DIRTY;

    LIST_INSERT_HEAD(&ko_list, ko, ko_link);

    *kp = ko;
    return 0;
}

static int
kobject_get_pagep(struct kobject *kp, uint64_t npage, void ***pp)
{
    if (npage < KOBJ_DIRECT_PAGES) {
	*pp = &kp->ko_pages[npage];
	return 0;
    }

    npage -= KOBJ_DIRECT_PAGES;
    if (npage < KOBJ_PAGES_PER_INDIR) {
	if (kp->ko_pages_indir1 == 0) {
	    int r = page_alloc((void**)&kp->ko_pages_indir1);
	    if (r < 0)
		return r;
	    memset(kp->ko_pages_indir1, 0, PGSIZE);
	}

	*pp = &kp->ko_pages_indir1[npage];
	return 0;
    }

    return -E_INVAL;
}

static void
kobject_free_pages(struct kobject *ko)
{
    for (int i = 0; i < KOBJ_DIRECT_PAGES; i++) {
	if (ko->ko_pages[i]) {
	    page_free(ko->ko_pages[i]);
	    ko->ko_pages[i] = 0;
	}
    }

    if (ko->ko_pages_indir1) {
	for (int i = 0; i < KOBJ_PAGES_PER_INDIR; i++) {
	    if (ko->ko_pages_indir1[i]) {
		page_free(ko->ko_pages_indir1[i]);
		ko->ko_pages_indir1[i] = 0;
	    }
	}
	page_free(ko->ko_pages_indir1);
	ko->ko_pages_indir1 = 0;
    }

    ko->ko_npages = 0;
}

int
kobject_get_page(struct kobject *kp, uint64_t npage, void **pp, kobj_rw_mode rw)
{
    if (npage > kp->ko_npages)
	return -E_INVAL;

    if (rw == kobj_rw && (kp->ko_flags & KOBJ_SNAPSHOTING)) {
	thread_suspend(cur_thread, &kobj_snapshot_waiting);
	return -E_RESTART;
    }

    if (rw == kobj_rw)
	kp->ko_flags |= KOBJ_DIRTY;

    void **kp_pagep;
    int r = kobject_get_pagep(kp, npage, &kp_pagep);
    if (r < 0)
	return r;

    *pp = *kp_pagep;
    return 0;
}

int
kobject_set_npages(struct kobject *kp, uint64_t npages)
{
    if (npages > KOBJ_DIRECT_PAGES + KOBJ_PAGES_PER_INDIR)
	return -E_NO_MEM;

    if ((kp->ko_flags & KOBJ_SNAPSHOTING)) {
	thread_suspend(cur_thread, &kobj_snapshot_waiting);
	return -E_RESTART;
    }

    kp->ko_flags |= KOBJ_DIRTY;

    for (int64_t i = npages; i < kp->ko_npages; i++) {
	void **pp;
	int r = kobject_get_pagep(kp, i, &pp);
	if (r < 0)
	    return r;

	page_free(*pp);
	*pp = 0;
    }

    for (int64_t i = kp->ko_npages; i < npages; i++) {
	void **pp;
	int r = kobject_get_pagep(kp, i, &pp);
	if (r < 0)
	    return r;

	if (*pp == 0)
	    r = page_alloc(pp);

	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (; i >= kp->ko_npages; i--) {
		int q = kobject_get_pagep(kp, i, &pp);
		if (q < 0)
		    panic("cannot lookup just-allocated page: %d", q);
		if (*pp) {
		    page_free(*pp);
		    *pp = 0;
		}
	    }
	    return r;
	}

	memset(*pp, 0, PGSIZE);
    }

    kp->ko_npages = npages;
    return 0;
}

void
kobject_swapin(struct kobject *ko)
{
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);
    ko->ko_pin = 0;
    ko->ko_flags &= ~(KOBJ_SNAPSHOTING | KOBJ_DIRTY);

    if (ko->ko_type == kobj_thread)
	thread_swapin((struct Thread *) ko);
    if (ko->ko_type == kobj_address_space)
	as_swapin((struct Address_space *) ko);
    if (ko->ko_type == kobj_segment)
	segment_swapin((struct Segment *) ko);
}

void
kobject_swapin_page(struct kobject *ko, uint64_t page_num, void *p)
{
    void **pp;
    int r = kobject_get_pagep(ko, page_num, &pp);
    if (r < 0)
	panic("cannot get slot for object %ld page %ld: %s",
	      ko->ko_id, page_num, e2s(r));

    *pp = p;
}

void
kobject_incref(struct kobject *ko)
{
    ko->ko_flags |= KOBJ_DIRTY;
    ++ko->ko_ref;
}

void
kobject_decref(struct kobject *ko)
{
    ko->ko_flags |= KOBJ_DIRTY;
    --ko->ko_ref;
}

void
kobject_incpin(struct kobject *ko)
{
    ++ko->ko_pin;
}

void
kobject_decpin(struct kobject *ko)
{
    --ko->ko_pin;
}

static void
kobject_gc(struct kobject *ko)
{
    int r = 0;

    switch (ko->ko_type) {
    case kobj_thread:
	r = thread_gc((struct Thread *) ko);
	break;

    case kobj_container:
	r = container_gc((struct Container *) ko);
	break;

    case kobj_address_space:
	r = as_gc((struct Address_space *) ko);
	break;

    case kobj_gate:
    case kobj_segment:
	break;

    default:
	panic("kobject_free: unknown kobject type %d", ko->ko_type);
    }

    if (r == -E_RESTART)
	return;
    if (r < 0)
	cprintf("kobject_free: cannot GC type %d: %d\n", ko->ko_type, r);

    kobject_free_pages(ko);
    ko->ko_type = kobj_dead;
    ko->ko_flags |= KOBJ_DIRTY;
}

void
kobject_gc_scan(void)
{
    // Clear cur_thread to avoid putting it to sleep on behalf of
    // our swapped-in objects.
    struct Thread *t = cur_thread;
    cur_thread = 0;

    struct kobject *ko;
    LIST_FOREACH(ko, &ko_list, ko_link)
	if (ko->ko_ref == 0 && ko->ko_pin == 0 && ko->ko_type != kobj_dead)
	    kobject_gc(ko);

    cur_thread = t;
}

void
kobject_swapout(struct kobject *ko)
{
    if (ko->ko_type == kobj_thread)
	thread_swapout((struct Thread *) ko);
    if (ko->ko_type == kobj_address_space)
	as_swapout((struct Address_space *) ko);

    LIST_REMOVE(ko, ko_link);
    kobject_free_pages(ko);
    page_free(ko);
}

struct kobject_buf *
kobject_get_snapbuf(struct kobject *ko)
{
    assert((ko->ko_flags & KOBJ_SNAPSHOTING));
    struct kobject_pair *kp = (struct kobject_pair *) ko;
    return &kp->snapshot;
}

void
kobject_snapshot(struct kobject *ko)
{
    assert(!(ko->ko_flags & KOBJ_SNAPSHOTING));

    if (ko->ko_type == kobj_segment)
	segment_snapshot((struct Segment *) ko);

    ko->ko_flags |= KOBJ_SNAPSHOTING;
    ko->ko_flags &= ~KOBJ_DIRTY;
    kobject_incpin(ko);

    struct kobject_buf *snap = kobject_get_snapbuf(ko);
    memcpy(snap, ko, sizeof(*snap));
}

void
kobject_snapshot_release(struct kobject *ko)
{
    assert((ko->ko_flags & KOBJ_SNAPSHOTING));
    ko->ko_flags &= ~KOBJ_SNAPSHOTING;
    kobject_decpin(ko);

    while (!LIST_EMPTY(&kobj_snapshot_waiting)) {
	struct Thread *t = LIST_FIRST(&kobj_snapshot_waiting);
	thread_set_runnable(t);
    }
}
