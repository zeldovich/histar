#include <kern/thread.h>
#include <kern/as.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <kern/timer.h>
#include <kern/ht.h>
#include <kern/pageinfo.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/cksum.h>

struct kobject_list ko_list;
struct Thread_list kobj_snapshot_waiting;

static HASH_TABLE(kobject_hash, struct kobject_list, kobj_hash_size) ko_hash;
static struct kobject_list ko_gc_list;

enum { kobject_reclaim_debug = 0 };
enum { kobject_checksum_enable = 0 };
enum { kobject_checksum_pedantic = 0 };
enum { kobject_print_sizes = 0 };

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
    uint32_t orig_flags = ko->ko_flags;
    struct kobject_hdr *mko = &kobject_const_h2k(ko)->hdr;
    mko->ko_flags &= ~(KOBJ_ON_DISK | KOBJ_SNAPSHOTING | KOBJ_SNAPSHOT_DIRTY |
		       KOBJ_SHARED_MAPPINGS | KOBJ_DIRTY_LATER);

    sum = cksum(sum, ko, offsetof(struct kobject_hdr, ko_sync_ts));
    sum = cksum(sum, (uint8_t *) ko + sizeof(struct kobject_hdr),
		     sizeof(struct kobject_persistent) -
		     sizeof(struct kobject_hdr));

    mko->ko_flags = orig_flags;

    for (uint64_t i = 0; i < ko->ko_nbytes; i += PGSIZE) {
	void *p;
	assert(0 == kobject_get_page(ko, i / PGSIZE, &p, page_shared_ro));
	assert(p);
	sum = cksum(sum, p, JMIN((uint32_t) PGSIZE, ko->ko_nbytes - i));
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
	cprintf("Missing label on object %"PRIu64" (%s)\n", ko->ko_id, ko->ko_name);
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
	r = label_compare_id(th_label_id, ko_label_id, label_leq_starlo) ? :
	    label_compare_id(ko_label_id, th_clear_id, label_leq_starlo);
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
kobject_incore(kobject_id_t id)
{
    struct kobject *ko;
    struct kobject_list *head = HASH_SLOT(&ko_hash, id);
    LIST_FOREACH(ko, head, ko_hash)
	if (ko->hdr.ko_id == id)
	    return 0;

    return -E_NOT_FOUND;
}

int
kobject_get(kobject_id_t id, const struct kobject **kp,
	    uint8_t type, info_flow_type iflow)
{
    id = kobject_translate_id(id);

    struct kobject *ko;
    struct kobject_list *head = HASH_SLOT(&ko_hash, id);
    LIST_FOREACH(ko, head, ko_hash) {
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
kobject_alloc(uint8_t type, const struct Label *l,
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
    static_assert(IS_POWER_OF_2(kobj_hash_size));

    struct kobject *ko = &ko_pair->active;
    memset(ko, 0, sizeof(struct kobject_mem));

    struct kobject_hdr *kh = &ko->hdr;
    kh->ko_type = type;
    kh->ko_id = handle_alloc();
    kh->ko_flags = KOBJ_DIRTY;
    kh->ko_quota_used = KOBJ_DISK_SIZE;
    kh->ko_quota_total = kh->ko_quota_used;

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
    struct kobject_list *hash_head = HASH_SLOT(&ko_hash, kh->ko_id);
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);
    LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);
    LIST_INSERT_HEAD(hash_head, ko, ko_hash);

    *kp = ko;
    return 0;
}

int
kobject_get_page(const struct kobject_hdr *kp, uint64_t npage, void **pp, page_sharing_mode rw)
{
    struct kobject *eko = kobject_ephemeral_dirty(kp);

    if (npage >= kobject_npages(kp))
	return -E_INVAL;

    if (SAFE_EQUAL(rw, page_excl_dirty))
	kobject_dirty(kp);

    if (SAFE_EQUAL(rw, page_excl_dirty_later)) {
	assert(eko->hdr.ko_type == kobj_segment);
	eko->hdr.ko_flags |= KOBJ_DIRTY_LATER;
    }

    int r = pagetree_get_page(&eko->ko_pt, npage, pp, rw);
    if (r >= 0) {
	if (*pp == 0)
	    panic("kobject_get_page: id %"PRIu64" (%s) type %d npage %"PRIu64" null",
		  kp->ko_id, kp->ko_name, kp->ko_type, npage);

	/*
	 * If copy-on-write happened, and we had some shared mappings earlier
	 * for a segment, we need to invalidate those previous (read-only)
	 * mappings to ensure they see the changes to this read-write page,
	 * which is now at a different address due to copy-on-write.
	 */
	if (r == 1 && eko->hdr.ko_type == kobj_segment &&
	    (eko->hdr.ko_flags & KOBJ_SHARED_MAPPINGS))
	{
	    eko->hdr.ko_flags &= ~KOBJ_SHARED_MAPPINGS;
	    segment_invalidate(&eko->sg, 0);
	}

	/*
	 * If this page is shared, we may need to go back and invalidate this
	 * segment's mappings later, to change the address of this page when
	 * a copy-on-write happens.
	 */
	struct page_info *ptp = page_to_pageinfo(*pp);
	if (ptp->pi_ref > 1 + ptp->pi_write_shared_ref)
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
	panic("kobject_dirty_eval: %"PRIu64" (%s) is not a segment: type %d\n",
	      ko->hdr.ko_id, ko->hdr.ko_name, ko->hdr.ko_type);

    segment_collect_dirty(&ko->sg);

    uint64_t npg = kobject_npages(&ko->hdr);
    for (uint64_t i = 0; i < npg; i++) {
	void *p;
	assert(0 == kobject_get_page(&ko->hdr, i, &p, page_shared_ro));

	struct page_info *pi = page_to_pageinfo(p);
	if (pi->pi_dirty) {
	    ko->hdr.ko_flags |= KOBJ_DIRTY;
	    return;
	}
    }
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

    if (!ko->ko_ref) {
	// If we have no parent, just increase our total quota -- when this
	// object is put into a container, ko_quota_total will add to the
	// container's ko_quota_used.
	ko->ko_quota_total += nbytes;
	return success;
    }

    assert(ko->ko_parent);

    const struct kobject *pconst = 0;
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

    if (npages < curnpg)
	assert(!kp->ko_pin_pg);

    for (uint64_t i = npages; i < curnpg; i++) {
	r = pagetree_put_page(&ko->ko_pt, i, 0);
	if (r < 0)
	    panic("kobject_set_nbytes: cannot drop page: %s", e2s(r));
    }

    uint64_t maxalloc = curnpg;
    for (uint64_t i = curnpg; i < npages; i++) {
	void *p;
	r = page_alloc(&p);
	if (r == 0) {
	    r = pagetree_put_page(&ko->ko_pt, i, p);
	    if (r < 0)
		page_free(p);
	    else
		maxalloc++;
	}

	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (uint64_t j = curnpg; j < maxalloc; j++)
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

    if (dsth->ko_ref) {
	cprintf("kobject_copy_pages: referenced segments not supported\n");
	return -E_INVAL;
    }

    r = pagetree_copy(&src->ko_pt, &dst->ko_pt, 0);
    if (r < 0)
	return r;

    dsth->ko_nbytes = srch->ko_nbytes;
    dsth->ko_quota_total += ROUNDUP(srch->ko_nbytes, PGSIZE);
    dsth->ko_quota_used += ROUNDUP(srch->ko_nbytes, PGSIZE);
    return 0;
}

struct kobject *
kobject_dirty(const struct kobject_hdr *kh)
{
    assert(kh);

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
	cprintf("kobject_swapin: %"PRIu64" (%s) checksum mismatch: 0x%"PRIx64" != 0x%"PRIx64"\n",
		ko->hdr.ko_id, ko->hdr.ko_name, sum1, sum2);

    struct kobject_list *hash_head = HASH_SLOT(&ko_hash, ko->hdr.ko_id);

    struct kobject *kx;
    LIST_FOREACH(kx, hash_head, ko_hash)
	if (ko->hdr.ko_id == kx->hdr.ko_id)
	    panic("kobject_swapin: duplicate %"PRIu64" (%s)",
		  ko->hdr.ko_id, ko->hdr.ko_name);

    kobject_negative_remove(ko->hdr.ko_id);
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);
    LIST_INSERT_HEAD(hash_head, ko, ko_hash);
    if (ko->hdr.ko_ref == 0)
	LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);

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
    if (ko->hdr.ko_ref == 0 && ko->hdr.ko_pin == 0)
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

    if (refholder->ko_id == ko->hdr.ko_parent)
	ko->hdr.ko_parent = 0;

    if (ko->hdr.ko_ref == 0 && ko->hdr.ko_pin == 0)
	LIST_INSERT_HEAD(&ko_gc_list, ko, ko_gc_link);

    // Inform threads so that they can halt, even if pinned (on zero refs)
    // or re-check that their parent refcounts are still readable.
    if (ko->hdr.ko_type == kobj_thread)
	thread_on_decref(&ko->th, refholder->ko_id);

    if (ko->hdr.ko_type == kobj_segment)
	segment_on_decref(&ko->sg, refholder->ko_id);
}

void
kobject_pin_hdr(const struct kobject_hdr *ko)
{
    struct kobject *m = kobject_const_h2k(ko);
    if (m->hdr.ko_ref == 0 && m->hdr.ko_pin == 0)
	LIST_REMOVE(m, ko_gc_link);
    ++m->hdr.ko_pin;
}

void
kobject_unpin_hdr(const struct kobject_hdr *ko)
{
    struct kobject *m = kobject_const_h2k(ko);
    if (m->hdr.ko_pin == 0)
	panic("kobject_unpin_hdr: not pinned: %"PRIu64" (%s)",
	      m->hdr.ko_id, m->hdr.ko_name);

    --m->hdr.ko_pin;
    if (m->hdr.ko_ref == 0 && m->hdr.ko_pin == 0)
	LIST_INSERT_HEAD(&ko_gc_list, m, ko_gc_link);
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
    case kobj_dead:
    case kobj_any:
    case kobj_ntypes:
	panic("kobject_gc: unknown kobject type %d", ko->hdr.ko_type);
    }

    if (r < 0)
	return r;

    for (int i = 0; i < kolabel_max; i++)
	kobject_set_label_prepared(&ko->hdr, i, l[i], 0, 0);

    pagetree_free(&ko->ko_pt, 0);
    ko->hdr.ko_nbytes = 0;
    ko->hdr.ko_type = kobj_dead;
    return 0;
}

void
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

	    if (ko->hdr.ko_ref || ko->hdr.ko_pin) {
		cprintf("kobject_gc_scan: referenced object on GC list: "
			"ref=%"PRIu64" pin=%d\n",
			ko->hdr.ko_ref, ko->hdr.ko_pin);
		continue;
	    }

	    if (ko->hdr.ko_type == kobj_dead) {
		if (!(ko->hdr.ko_flags & KOBJ_ON_DISK))
		    kobject_swapout(ko);
	    } else {
		int r = kobject_gc(kobject_dirty(&ko->hdr));
		if (r >= 0)
		    progress = 1;
		if (r < 0 && r != -E_RESTART)
		    cprintf("kobject_gc_scan: %"PRIu64" type %d: %s\n",
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

	if (ko->hdr.ko_ref && sum1 != sum2)
	    cprintf("kobject_swapout: %"PRIu64" (%s) checksum mismatch: 0x%"PRIx64" != 0x%"PRIx64"\n",
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
    if (ko->hdr.ko_ref == 0 && ko->hdr.ko_pin == 0)
	LIST_REMOVE(ko, ko_gc_link);
    LIST_REMOVE(ko, ko_hash);
    pagetree_free(&ko->ko_pt, 0);
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
	if (sum != snap->hdr.ko_cksum)
	    cprintf("kobject_get_snapshot(%"PRIu64", %s): cksum changed 0x%"PRIx64" -> 0x%"PRIx64"\n",
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

    ko->ko_flags &= ~(KOBJ_DIRTY | KOBJ_SNAP_SHARE_PIN);
    ko->ko_cksum = kobject_cksum(ko);
    kobject_pin_hdr(ko);

    struct kobject *snap = kobject_get_snapshot_internal(ko);
    memcpy(snap, ko, KOBJ_DISK_SIZE);

    int share_pinned = 0;
    assert(0 == pagetree_copy(&kobject_h2k(ko)->ko_pt, &snap->ko_pt,
			      &share_pinned));

    if (share_pinned)
	snap->hdr.ko_flags |= KOBJ_SNAP_SHARE_PIN;

    ko->ko_flags |= KOBJ_SNAPSHOTING;
}

void
kobject_snapshot_release(struct kobject_hdr *ko)
{
    struct kobject *snap = kobject_get_snapshot(ko);

    ko->ko_flags &= ~KOBJ_SNAPSHOTING;
    kobject_unpin_hdr(ko);
    pagetree_free(&snap->ko_pt, !!(snap->hdr.ko_flags & KOBJ_SNAP_SHARE_PIN));

    while (!LIST_EMPTY(&kobj_snapshot_waiting)) {
	struct Thread *t = LIST_FIRST(&kobj_snapshot_waiting);
	thread_set_runnable(t);
    }
}

// Negative kobject id cache (objects that don't exist)
static struct {
    int next;
    kobject_id_t ents[kobj_neg_size];
} kobject_neg[kobj_neg_hash];

void
kobject_negative_insert(kobject_id_t id)
{
    uint64_t idx = id % kobj_neg_hash;
    kobject_neg[idx].ents[kobject_neg[idx].next] = id;
    kobject_neg[idx].next = (kobject_neg[idx].next + 1) % kobj_neg_size;
}

void
kobject_negative_remove(kobject_id_t id)
{
    uint64_t idx = id % kobj_neg_hash;
    for (int i = 0; i < kobj_neg_size; i++)
	if (kobject_neg[idx].ents[i] == id)
	    kobject_neg[idx].ents[i] = 0;
}

bool_t
kobject_negative_contains(kobject_id_t id)
{
    uint64_t idx = id % kobj_neg_hash;
    for (int i = 0; i < kobj_neg_size; i++)
	if (kobject_neg[idx].ents[i] == id)
	    return 1;
    return 0;
}

bool_t
kobject_initial(const struct kobject *ko)
{
    if (ko->hdr.ko_ref == 0)
	return 1;

    if (ko->hdr.ko_type == kobj_thread)
	return SAFE_EQUAL(ko->th.th_status, thread_runnable) ||
	       SAFE_EQUAL(ko->th.th_status, thread_suspended);

    return 0;
}

void
kobject_reclaim(void)
{
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
	    cprintf("kobject_reclaim: swapping out %"PRIu64" (%s)\n",
		    ko->hdr.ko_id, ko->hdr.ko_name);

	kobject_swapout(ko);
    }

    cur_thread = t;
}

void
kobject_init(void)
{
    static struct periodic_task gc_pt =
	{ .pt_fn = &kobject_gc_scan, .pt_interval_sec = 1 };
    timer_add_periodic(&gc_pt);

    if (kobject_print_sizes) {
	cprintf("kobject sizes:\n");
	cprintf("hdr %lu label %lu pagetree %lu trapframe %lu\n",
		(long) sizeof(struct kobject_hdr),
		(long) sizeof(struct Label),
		(long) sizeof(struct pagetree),
		(long) sizeof(struct Trapframe));
	cprintf("ct %lu th %lu gt %lu as %lu sg %lu\n",
		(long) sizeof(struct Container),
		(long) sizeof(struct Thread),
		(long) sizeof(struct Gate),
		(long) sizeof(struct Address_space),
		(long) sizeof(struct Segment));
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
	    uint64_t need = ko->ko_quota_total - quota_avail;
	    int r = kobject_borrow_parent_quota(qr->qr_ko, need);
	    if (r == -E_RESTART)
		return r;

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
