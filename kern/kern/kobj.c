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
#include <kern/ht.h>
#include <kern/pageinfo.h>
#include <inc/error.h>
#include <inc/cksum.h>

struct kobject_list ko_list;
struct Thread_list kobj_snapshot_waiting;

static HASH_TABLE(kobject_hash, struct kobject_list, 8191) ko_hash;
static struct kobject_list ko_gc_list;

static int kobject_reclaim_debug = 0;
static int kobject_checksum_enable = 0;
static int kobject_checksum_pedantic = 0;
static int kobject_print_sizes = 0;

struct kobject *
kobject_h2k(struct kobject_hdr *kh)
{
    return (struct kobject *) kh;
}

const struct kobject *
kobject_ch2ck(const struct kobject_hdr *kh)
{
    return (const struct kobject *) kh;
}

static struct kobject *
kobject_const_h2k(const struct kobject_hdr *kh)
{
    return (struct kobject *) kh;
}

static uint64_t
kobject_cksum(const struct kobject_hdr *ko)
{
    if (!kobject_checksum_enable)
	return 0;

    assert(ko);
    uint64_t sum = 0;

    // Compute checksum on the persistent parts of kobject_hdr
    sum = cksum(sum, ko, offsetof(struct kobject_hdr, ko_cksum));
    sum = cksum(sum, (uint8_t *) ko + sizeof(struct kobject_hdr),
		     sizeof(struct kobject_persistent) -
		     sizeof(struct kobject_hdr));

    for (uint64_t i = 0; i < ko->ko_nbytes; i += PGSIZE) {
	void *p;
	assert(0 == kobject_get_page(ko, i / PGSIZE, &p, page_shared_ro));
	assert(p);
	sum = cksum(sum, p, MIN((uint32_t) PGSIZE, ko->ko_nbytes - i * PGSIZE));
    }

    return sum;
}

static int
kobject_iflow_check(const struct kobject_hdr *ko, info_flow_type iflow)
{
    if (SAFE_EQUAL(iflow, iflow_none))
	return 0;

    if (cur_thread == 0)
	return 0;

    kobject_id_t th_label_id = cur_thread->th_ko.ko_label[kolabel_contaminate];
    kobject_id_t th_clear_id = cur_thread->th_ko.ko_label[kolabel_clearance];
    kobject_id_t ko_label_id = ko->ko_label[kolabel_contaminate];

    assert(th_label_id && th_clear_id);
    if (ko_label_id == 0) {
	cprintf("Missing label on object %ld (%s)\n", ko->ko_id, ko->ko_name);
	return -E_LABEL;
    }

    int r = 0;
    if (SAFE_EQUAL(iflow, iflow_read)) {
	r = label_compare_id(ko_label_id, th_label_id, label_leq_starhi);
    } else if (SAFE_EQUAL(iflow, iflow_rw)) {
	r = (ko->ko_flags & KOBJ_READONLY) ? -E_LABEL :
	    label_compare_id(th_label_id, ko_label_id, label_leq_starlo) ? :
	    label_compare_id(ko_label_id, th_label_id, label_leq_starhi);
    } else if (SAFE_EQUAL(iflow, iflow_alloc)) {
	r = label_compare_id(th_label_id, ko_label_id, label_leq_starlo);
	//r = label_compare_id(th_label_id, ko_label_id, label_leq_starlo) ? :
	//    label_compare_id(ko_label_id, th_clear_id, label_leq_starlo);
    } else {
	panic("kobject_get: unknown iflow type %d", SAFE_UNWRAP(iflow));
    }

    return r;
}

static kobject_id_t
kobject_translate_id(kobject_id_t id)
{
    if (id == kobject_id_thread_sg)
	id = cur_thread->th_sg;

    return id;
}

int
kobject_get(kobject_id_t id, const struct kobject **kp,
	    kobject_type_t type, info_flow_type iflow)
{
    id = kobject_translate_id(id);

    struct kobject *ko;
    LIST_FOREACH(ko, HASH_SLOT(&ko_hash, id), ko_hash) {
	if (ko->hdr.ko_id == id) {
	    if (ko->hdr.ko_ref == 0)
		return -E_INVAL;

	    int r = ko->hdr.ko_type == kobj_label ? 0 :
		    kobject_iflow_check(&ko->hdr, iflow);
	    if (r < 0)
		return r;

	    if (type != kobj_any && type != ko->hdr.ko_type)
		return -E_INVAL;

	    *kp = ko;
	    return 0;
	}
    }

    // Should match the code returned above when trying
    // to get an object of the wrong type.
    if (kobject_negative_contains(id))
	return -E_INVAL;

    return pstate_swapin(id);
}

int
kobject_get_label(const struct kobject_hdr *kp, int idx,
		  const struct Label **lpp)
{
    struct kobject *ko = kobject_const_h2k(kp);
    const struct kobject *label_ko;

    label_ko = ko->ko_label_cache[idx].wp_kobj;
    if (label_ko && label_ko->hdr.ko_id == kp->ko_label[idx]) {
	*lpp = &label_ko->lb;
	return 0;
    }

    if (kp->ko_label[idx]) {
	int r = kobject_get(kp->ko_label[idx], &label_ko, kobj_label, iflow_none);
	if (r < 0)
	    return r;

	kobj_weak_put(&ko->ko_label_cache[idx], kobject_const_h2k(&label_ko->hdr));
	*lpp = &label_ko->lb;
	return 0;
    }

    *lpp = 0;
    return 0;
}

int
kobject_set_label(struct kobject_hdr *kp, int idx,
		  const struct Label *new_label)
{
    const struct Label *old_label;
    int r = kobject_get_label(kp, idx, &old_label);
    if (r < 0)
	return r;

    struct kobject_quota_resv qr;
    kobject_qres_init(&qr, kp);
    if (new_label) {
	r = kobject_qres_reserve(&qr, &new_label->lb_ko);
	if (r < 0)
	    return r;
    }

    kobject_set_label_prepared(kp, idx, old_label, new_label, &qr);
    return 0;
}

void
kobject_set_label_prepared(struct kobject_hdr *kp, int idx,
			   const struct Label *old_label,
			   const struct Label *new_label,
			   struct kobject_quota_resv *qr)
{
    if (old_label) {
	assert(kp->ko_label[idx] == old_label->lb_ko.ko_id);
	kobject_decref(&old_label->lb_ko, kp);
    } else {
	assert(kp->ko_label[idx] == 0);
    }

    if (new_label)
	kobject_incref_resv(&new_label->lb_ko, qr);

    kp->ko_label[idx] = new_label ? new_label->lb_ko.ko_id : 0;
}

int
kobject_alloc(kobject_type_t type, const struct Label *l,
	      struct kobject **kp)
{
    assert(type == kobj_label || l != 0);

    void *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct kobject_pair *ko_pair = (struct kobject_pair *) p;
    static_assert(sizeof(struct kobject) == KOBJ_MEM_SIZE);
    static_assert(sizeof(struct kobject_persistent) == KOBJ_DISK_SIZE);
    static_assert(sizeof(*ko_pair) <= PGSIZE);

    struct kobject *ko = &ko_pair->active;
    memset(ko, 0, sizeof(*ko));

    struct kobject_hdr *kh = &ko->hdr;
    kh->ko_type = type;
    kh->ko_id = handle_alloc();
    kh->ko_flags = KOBJ_DIRTY;
    kh->ko_quota_used = KOBJ_DISK_SIZE;
    kh->ko_quota_total = kh->ko_quota_used;
    pagetree_init(&ko->ko_pt);

    r = kobject_set_label(kh, kolabel_contaminate, l);
    if (r < 0) {
	page_free(p);
	return r;
    }

    // Make sure that it's legal to allocate object at this label!
    r = l ? kobject_iflow_check(kh, iflow_alloc) : 0;
    if (r < 0) {
	kobject_set_label_prepared(kh, kolabel_contaminate, l, 0, 0);
	page_free(p);
	return r;
    }

    kobject_negative_remove(kh->ko_id);
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);
    LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);
    LIST_INSERT_HEAD(HASH_SLOT(&ko_hash, kh->ko_id), ko, ko_hash);

    *kp = ko;
    return 0;
}

int
kobject_get_page(const struct kobject_hdr *kp, uint64_t npage, void **pp, page_sharing_mode rw)
{
    if (npage >= kobject_npages(kp))
	return -E_INVAL;

    // If this is a segment, and we have shared pages mapped somewhere,
    // and rw is not shared, we need to invalidate all of the mappings
    // to those shared pages.
    struct kobject *eko = kobject_ephemeral_dirty(kp);
    if (eko->hdr.ko_type == kobj_segment &&
	(eko->hdr.ko_flags & KOBJ_SHARED_MAPPINGS) &&
	!SAFE_EQUAL(rw, page_shared_ro))
    {
	eko->hdr.ko_flags &= ~KOBJ_SHARED_MAPPINGS;
	segment_invalidate(&eko->sg);
    }

    if (SAFE_EQUAL(rw, page_excl_dirty))
	kobject_dirty(kp);
    if (SAFE_EQUAL(rw, page_excl_dirty_later)) {
	assert(eko->hdr.ko_type == kobj_segment);
	eko->hdr.ko_flags |= KOBJ_DIRTY_LATER;
    }

    int r = pagetree_get_page(&kobject_const_h2k(kp)->ko_pt,
			      npage, pp, rw);
    if (r == 0) {
	if (*pp == 0)
	    panic("kobject_get_page: id %ld (%s) type %d npage %ld null",
		  kp->ko_id, kp->ko_name, kp->ko_type, npage);

	// Be conservative -- assume caller will map this page somewhere..
	if (page_to_pageinfo(*pp)->pi_ref > 1)
	    eko->hdr.ko_flags |= KOBJ_SHARED_MAPPINGS;
    }
    return r;
}

void
kobject_dirty_eval(struct kobject *ko)
{
    if (!(ko->hdr.ko_flags & KOBJ_DIRTY_LATER))
	return;
    ko->hdr.ko_flags &= ~KOBJ_DIRTY_LATER;

    if (ko->hdr.ko_type != kobj_segment)
	panic("kobject_dirty_eval: %ld (%s) is not a segment: type %d\n",
	      ko->hdr.ko_id, ko->hdr.ko_name, ko->hdr.ko_type);

    segment_collect_dirty(&ko->sg);

    int dirty = 0;
    uint64_t npg = kobject_npages(&ko->hdr);
    for (uint64_t i = 0; i < npg; i++) {
	void *p;
	assert(0 == kobject_get_page(&ko->hdr, i, &p, page_shared_ro));

	struct page_info *pi = page_to_pageinfo(p);
	if (pi->pi_dirty)
	    dirty = 1;
    }

    if (dirty)
	ko->hdr.ko_flags |= KOBJ_DIRTY;
}

uint64_t
kobject_npages(const struct kobject_hdr *kp)
{
    return ROUNDUP(kp->ko_nbytes, PGSIZE) / PGSIZE;
}

static int
kobject_borrow_parent_quota_one(struct kobject_hdr *ko, uint64_t nbytes)
{
    int success = 0;

again:
    if ((ko->ko_flags & KOBJ_FIXED_QUOTA))
	return -E_FIXED_QUOTA;

    if (!ko->ko_parent) {
	// If we have no parent, just increase our total quota -- when this
	// object is put into a container, ko_quota_total will add to the
	// container's ko_quota_used.
	ko->ko_quota_total += nbytes;
	return success;
    }

    const struct kobject *pconst;
    int r = kobject_get(ko->ko_parent, &pconst, kobj_any, iflow_rw);
    if (r < 0)
	return r;

    struct kobject_hdr *parent = &kobject_dirty(&pconst->hdr)->hdr;
    if (parent->ko_quota_total != CT_QUOTA_INF &&
	parent->ko_quota_total - parent->ko_quota_used < nbytes)
    {
	nbytes = nbytes - (parent->ko_quota_total - parent->ko_quota_used);
	ko = parent;
	success = -E_AGAIN;
	goto again;
    }

    parent->ko_quota_used += nbytes;
    ko->ko_quota_total += nbytes;
    return success;
}

static int
kobject_borrow_parent_quota(struct kobject_hdr *ko, uint64_t nbytes)
{
    int r;

    do {
	r = kobject_borrow_parent_quota_one(ko, nbytes);
    } while (r == -E_AGAIN);

    return r;
}

int
kobject_set_nbytes(struct kobject_hdr *kp, uint64_t nbytes)
{
    int r;
    struct kobject *ko = kobject_h2k(kp);

    uint64_t curnpg = kobject_npages(kp);
    uint64_t npages = ROUNDUP(nbytes, PGSIZE) / PGSIZE;
    if (npages > pagetree_maxpages())
	return -E_RANGE;

    int64_t quota_diff = (npages - curnpg) * PGSIZE;
    uint64_t abs_qdiff = (quota_diff > 0) ? quota_diff : -quota_diff;
    if (quota_diff > 0 &&
	kp->ko_quota_total != CT_QUOTA_INF &&
	kp->ko_quota_total - kp->ko_quota_used < abs_qdiff)
    {
	// Try to borrow quota from parent container..
	r = kobject_borrow_parent_quota(kp, abs_qdiff);
	if (r < 0) {
	    cprintf("kobject_set_nbytes: borrow quota: %s\n", e2s(r));
	    return -E_RESOURCE;
	}
    }

    for (uint64_t i = npages; i < curnpg; i++) {
	r = pagetree_put_page(&ko->ko_pt, i, 0);
	if (r < 0)
	    panic("kobject_set_nbytes: cannot drop page: %s", e2s(r));
    }

    for (uint64_t i = curnpg; i < npages; i++) {
	void *p;
	r = page_alloc(&p);
	if (r == 0)
	    r = pagetree_put_page(&ko->ko_pt, i, p);

	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (uint64_t j = kobject_npages(kp); j < i; j++)
		assert(0 == pagetree_put_page(&ko->ko_pt, j, 0));
	    return r;
	}

	memset(p, 0, PGSIZE);
    }

    kp->ko_nbytes = nbytes;
    kp->ko_quota_used += quota_diff;
    return 0;
}

int
kobject_copy_pages(const struct kobject_hdr *srch,
		   struct kobject_hdr *dsth)
{
    const struct kobject *src = kobject_ch2ck(srch);
    struct kobject *dst = kobject_h2k(dsth);

    int r = kobject_set_nbytes(dsth, 0);
    if (r < 0)
	return r;

    if (dsth->ko_parent) {
	cprintf("kobject_copy_pages: referenced segments not supported\n");
	return -E_INVAL;
    }

    pagetree_copy(&src->ko_pt, &dst->ko_pt);
    dsth->ko_nbytes = srch->ko_nbytes;
    dsth->ko_quota_total += ROUNDUP(srch->ko_nbytes, PGSIZE);
    dsth->ko_quota_used += ROUNDUP(srch->ko_nbytes, PGSIZE);
    return 0;
}

struct kobject *
kobject_dirty(const struct kobject_hdr *kh)
{
    struct kobject *ko = kobject_const_h2k(kh);
    ko->hdr.ko_flags |= KOBJ_DIRTY;
    return ko;
}

struct kobject *
kobject_ephemeral_dirty(const struct kobject_hdr *kh)
{
    return kobject_const_h2k(kh);
}

void
kobject_swapin(struct kobject *ko)
{
    assert(ko->hdr.ko_id);

    uint64_t sum1 = ko->hdr.ko_cksum;
    uint64_t sum2 = kobject_cksum(&ko->hdr);

    if (sum1 != sum2)
	cprintf("kobject_swapin: %ld (%s) checksum mismatch: 0x%lx != 0x%lx\n",
		ko->hdr.ko_id, ko->hdr.ko_name, sum1, sum2);

    struct kobject *kx;
    LIST_FOREACH(kx, &ko_list, ko_link)
	if (ko->hdr.ko_id == kx->hdr.ko_id)
	    panic("kobject_swapin: duplicate %ld (%s)",
		  ko->hdr.ko_id, ko->hdr.ko_name);

    kobject_negative_remove(ko->hdr.ko_id);
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);
    if (ko->hdr.ko_ref == 0)
	LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);
    LIST_INSERT_HEAD(HASH_SLOT(&ko_hash, ko->hdr.ko_id), ko, ko_hash);

    ko->hdr.ko_pin = 0;
    ko->hdr.ko_pin_pg = 0;
    ko->hdr.ko_flags &= ~(KOBJ_SNAPSHOTING | KOBJ_DIRTY |
			  KOBJ_SHARED_MAPPINGS | KOBJ_SNAPSHOT_DIRTY);

    if (ko->hdr.ko_type == kobj_thread)
	thread_swapin(&ko->th);
    if (ko->hdr.ko_type == kobj_address_space)
	as_swapin(&ko->as);
    if (ko->hdr.ko_type == kobj_segment)
	segment_swapin(&ko->sg);
}

int
kobject_incref(const struct kobject_hdr *kh, struct kobject_hdr *refholder)
{
    struct kobject_quota_resv qr;

    kobject_qres_init(&qr, refholder);
    int r = kobject_qres_reserve(&qr, kh);
    if (r < 0)
	return r;

    kobject_incref_resv(kh, &qr);
    return 0;
}

void
kobject_incref_resv(const struct kobject_hdr *kh, struct kobject_quota_resv *qr)
{
    struct kobject *ko = kobject_dirty(kh);
    if (ko->hdr.ko_ref == 0)
	LIST_REMOVE(ko, ko_gc_link);
    ko->hdr.ko_ref++;

    if (qr) {
	kobject_qres_take(qr, kh);
	ko->hdr.ko_parent = qr->qr_ko->ko_id;
    }
}

void
kobject_decref(const struct kobject_hdr *kh, struct kobject_hdr *refholder)
{
    struct kobject *ko = kobject_dirty(kh);
    assert(ko->hdr.ko_ref);
    ko->hdr.ko_ref--;
    refholder->ko_quota_used -= kh->ko_quota_total;

    if (ko->hdr.ko_ref == 0) {
	LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);

	// Inform threads so that they can halt, even if pinned
	if (ko->hdr.ko_type == kobj_thread)
	    thread_zero_refs(&ko->th);
    }
}

void
kobject_pin_hdr(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->hdr;
    ++m->ko_pin;
}

void
kobject_unpin_hdr(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->hdr;
    --m->ko_pin;

    if (m->ko_pin == (uint32_t) -1)
	panic("kobject_unpin_hdr: underflow for object %ld (%s)",
	      m->ko_id, m->ko_name);
}

void
kobject_pin_page(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->hdr;
    ++m->ko_pin_pg;

    kobject_pin_hdr(ko);
}

void
kobject_unpin_page(const struct kobject_hdr *ko)
{
    struct kobject_hdr *m = &kobject_const_h2k(ko)->hdr;
    if (m->ko_pin_pg == 0)
	panic("kobject_unpin_page: not pinned");
    --m->ko_pin_pg;

    kobject_unpin_hdr(ko);
}

static int
kobject_gc(struct kobject *ko)
{
    int r;
    const struct Label *l[kolabel_max];
    for (int i = 0; i < kolabel_max; i++) {
	r = kobject_get_label(&ko->hdr, i, &l[i]);
	if (r < 0)
	    return r;
    }

    if ((ko->hdr.ko_flags & KOBJ_DIRTY_LATER))
	kobject_dirty_eval(ko);

    switch (ko->hdr.ko_type) {
    case kobj_thread:
	r = thread_gc(&ko->th);
	break;

    case kobj_container:
	r = container_gc(&ko->ct);
	break;

    case kobj_address_space:
	r = as_gc(&ko->as);
	break;

    case kobj_gate:
    case kobj_netdev:
    case kobj_segment:
    case kobj_label:
	break;

    default:
	panic("kobject_gc: unknown kobject type %d", ko->hdr.ko_type);
    }

    if (r < 0)
	return r;

    for (int i = 0; i < kolabel_max; i++)
	kobject_set_label_prepared(&ko->hdr, i, l[i], 0, 0);

    pagetree_free(&ko->ko_pt);
    ko->hdr.ko_nbytes = 0;
    ko->hdr.ko_type = kobj_dead;
    return 0;
}

static void
kobject_gc_scan(void)
{
    // Clear cur_thread to avoid putting it to sleep on behalf of
    // our swapped-in objects.
    const struct Thread *t = cur_thread;
    cur_thread = 0;

    // Keep running GC until we make no more progress.  This is so that
    // all objects in a zero-ref container get GCed in the same pass.
    int progress;

    do {
	progress = 0;

	struct kobject *ko, *next;
	for (ko = LIST_FIRST(&ko_gc_list); ko; ko = next) {
	    next = LIST_NEXT(ko, ko_gc_link);

	    if (ko->hdr.ko_ref) {
		cprintf("kobject_gc_scan: referenced object on GC list!\n");
		continue;
	    }

	    if (ko->hdr.ko_pin)
		continue;

	    if (ko->hdr.ko_type == kobj_dead) {
		if (!(ko->hdr.ko_flags & KOBJ_ON_DISK))
		    kobject_swapout(ko);
	    } else {
		int r = kobject_gc(kobject_dirty(&ko->hdr));
		if (r >= 0)
		    progress = 1;
		if (r < 0 && r != -E_RESTART)
		    cprintf("kobject_gc_scan: %ld type %d: %s\n",
			    ko->hdr.ko_id, ko->hdr.ko_type, e2s(r));
	    }
	}
    } while (progress);

    cur_thread = t;
}

void
kobject_swapout(struct kobject *ko)
{
    if (kobject_checksum_pedantic) {
	uint64_t sum1 = ko->hdr.ko_cksum;
	uint64_t sum2 = kobject_cksum(&ko->hdr);

	if (sum1 != sum2)
	    cprintf("kobject_swapout: %ld (%s) checksum mismatch: 0x%lx != 0x%lx\n",
		    ko->hdr.ko_id, ko->hdr.ko_name, sum1, sum2);
    }

    assert(ko->hdr.ko_pin == 0);
    assert(!(ko->hdr.ko_flags & KOBJ_SNAPSHOTING));

    if (ko->hdr.ko_type == kobj_thread)
	thread_swapout(&ko->th);
    if (ko->hdr.ko_type == kobj_address_space)
	as_swapout(&ko->as);

    for (int i = 0; i < kolabel_max; i++)
	kobj_weak_put(&ko->ko_label_cache[i], 0);
    kobj_weak_drop(&ko->ko_weak_refs);

    LIST_REMOVE(ko, ko_link);
    if (ko->hdr.ko_ref == 0)
	LIST_REMOVE(ko, ko_gc_link);
    LIST_REMOVE(ko, ko_hash);
    pagetree_free(&ko->ko_pt);
    page_free(ko);
}

static struct kobject *
kobject_get_snapshot_internal(struct kobject_hdr *ko)
{
    struct kobject_pair *kp = (struct kobject_pair *) ko;
    return &kp->snapshot;
}

struct kobject *
kobject_get_snapshot(struct kobject_hdr *ko)
{
    assert((ko->ko_flags & KOBJ_SNAPSHOTING));
    struct kobject *snap = kobject_get_snapshot_internal(ko);

    if (kobject_checksum_pedantic) {
	uint64_t sum = kobject_cksum(&snap->hdr);
	if (sum != ko->ko_cksum)
	    cprintf("kobject_get_snapshot(%ld, %s): cksum changed 0x%lx -> 0x%lx\n",
		    ko->ko_id, ko->ko_name, ko->ko_cksum, sum);
    }

    return snap;
}

void
kobject_snapshot(struct kobject_hdr *ko)
{
    assert(!(ko->ko_flags & KOBJ_SNAPSHOTING));

    if (ko->ko_type == kobj_segment)
	segment_map_ro(&kobject_h2k(ko)->sg);

    ko->ko_flags &= ~KOBJ_DIRTY;
    ko->ko_cksum = kobject_cksum(ko);
    kobject_pin_hdr(ko);

    struct kobject *snap = kobject_get_snapshot_internal(ko);
    memcpy(snap, ko, KOBJ_DISK_SIZE);
    pagetree_copy(&kobject_h2k(ko)->ko_pt, &snap->ko_pt);

    ko->ko_flags |= KOBJ_SNAPSHOTING;
}

void
kobject_snapshot_release(struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    ko->ko_flags &= ~KOBJ_SNAPSHOTING;
    kobject_unpin_hdr(ko);
    pagetree_free(&snap->ko_pt);

    while (!LIST_EMPTY(&kobj_snapshot_waiting)) {
	struct Thread *t = LIST_FIRST(&kobj_snapshot_waiting);
	thread_set_runnable(t);
    }
}

// Negative kobject id cache (objects that don't exist)
enum { kobject_neg_nent = 16 };
static struct {
    int next;
    kobject_id_t ents[kobject_neg_nent];
} kobject_neg;

void
kobject_negative_insert(kobject_id_t id)
{
    kobject_neg.ents[kobject_neg.next] = id;
    kobject_neg.next = (kobject_neg.next + 1) % kobject_neg_nent;
}

void
kobject_negative_remove(kobject_id_t id)
{
    for (int i = 0; i < kobject_neg_nent; i++)
	if (kobject_neg.ents[i] == id)
	    kobject_neg.ents[i] = 0;
}

bool_t
kobject_negative_contains(kobject_id_t id)
{
    for (int i = 0; i < kobject_neg_nent; i++)
	if (kobject_neg.ents[i] == id)
	    return 1;
    return 0;
}

bool_t
kobject_initial(const struct kobject *ko)
{
    if ((ko->hdr.ko_flags & KOBJ_PIN_IDLE))
	return 1;

    if (ko->hdr.ko_ref == 0)
	return 1;

    if (ko->hdr.ko_type == kobj_thread)
	return SAFE_EQUAL(ko->th.th_status, thread_runnable) ||
	       SAFE_EQUAL(ko->th.th_status, thread_suspended);

    return 0;
}

static int
kobject_reclaim_check(void)
{
    // A rather simple heuristic for when to clean up
    return (page_stats.pages_avail < global_npages / 4);
}

static void
kobject_reclaim(void)
{
    if (!kobject_reclaim_check())
	return;

    const struct Thread *t = cur_thread;
    cur_thread = 0;

    struct kobject *next;
    for (struct kobject *ko = LIST_FIRST(&ko_list); ko != 0; ko = next) {
	next = LIST_NEXT(ko, ko_link);

	if (ko->hdr.ko_pin || kobject_initial(ko))
	    continue;
	if ((ko->hdr.ko_flags & (KOBJ_DIRTY | KOBJ_SNAPSHOT_DIRTY)))
	    continue;

	if (kobject_reclaim_debug)
	    cprintf("kobject_reclaim: swapping out %ld (%s)\n",
		    ko->hdr.ko_id, ko->hdr.ko_name);

	kobject_swapout(ko);
    }

    if (kobject_reclaim_check()) {
	cprintf("kobject_reclaim: unable to reclaim much memory\n");
	cprintf("kobject_reclaim: used %ld avail %ld alloc %ld fail %ld\n",
		page_stats.pages_used, page_stats.pages_avail,
		page_stats.allocations, page_stats.failures);
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

    if (kobject_print_sizes) {
	cprintf("kobject sizes:\n");
	cprintf("hdr %ld label %ld pagetree %ld\n",
		sizeof(struct kobject_hdr),
		sizeof(struct Label),
		sizeof(struct pagetree));
	cprintf("ct %ld th %ld gt %ld as %ld sg %ld\n",
		sizeof(struct Container),
		sizeof(struct Thread),
		sizeof(struct Gate),
		sizeof(struct Address_space),
		sizeof(struct Segment));
    }
}

void
kobject_qres_init(struct kobject_quota_resv *qr, struct kobject_hdr *ko)
{
    assert(ko);
    qr->qr_ko = ko;
    qr->qr_nbytes = 0;
}

int
kobject_qres_reserve(struct kobject_quota_resv *qr, const struct kobject_hdr *ko)
{
    assert(ko);

    if (qr->qr_ko->ko_quota_total != CT_QUOTA_INF) {
	uint64_t quota_avail = qr->qr_ko->ko_quota_total - qr->qr_ko->ko_quota_used;
	if (quota_avail < ko->ko_quota_total) {
	    int r = kobject_borrow_parent_quota(qr->qr_ko,
						ko->ko_quota_total - quota_avail);
	    if (r < 0) {
		cprintf("kobject_qres_reserve: borrow parent: %s\n", e2s(r));
		return -E_RESOURCE;
	    }
	}

	qr->qr_ko->ko_quota_used += ko->ko_quota_total;
	qr->qr_nbytes += ko->ko_quota_total;
    }

    return 0;
}

void
kobject_qres_take(struct kobject_quota_resv *qr, const struct kobject_hdr *ko)
{
    assert(ko);

    if (qr->qr_ko->ko_quota_total != CT_QUOTA_INF) {
	assert(qr->qr_nbytes >= ko->ko_quota_total);
	qr->qr_nbytes -= ko->ko_quota_total;
    }
}

void
kobject_qres_release(struct kobject_quota_resv *qr)
{
    qr->qr_ko->ko_quota_used -= qr->qr_nbytes;
}
