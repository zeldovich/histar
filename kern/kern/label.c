#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <inc/safeint.h>

////////////////////
// Label handling
////////////////////

static uint32_t
label_nslots(const struct Label *l)
{
    return NUM_LB_ENT_INLINE + kobject_npages(&l->lb_ko) * NUM_LB_ENT_PER_PAGE;
}

static int
label_get_slotp_read(const struct Label *l, uint32_t slotnum,
		     const uint64_t **slotp)
{
    if (slotnum < NUM_LB_ENT_INLINE) {
	*slotp = &l->lb_ent[slotnum];
	return 0;
    }

    slotnum -= NUM_LB_ENT_INLINE;
    const struct Label_page *lpg;
    int r = kobject_get_page(&l->lb_ko, slotnum / NUM_LB_ENT_PER_PAGE,
			     (void **) &lpg, page_shared_ro);
    if (r < 0)
	return r;

    *slotp = &lpg->lp_ent[slotnum % NUM_LB_ENT_PER_PAGE];
    return 0;
}

static int
label_get_slotp_write(struct Label *l, uint32_t slotnum, uint64_t **slotp)
{
    if (slotnum >= l->lb_nent)
	l->lb_nent = slotnum + 1;

    if (slotnum < NUM_LB_ENT_INLINE) {
	*slotp = &l->lb_ent[slotnum];
	return 0;
    }

    slotnum -= NUM_LB_ENT_INLINE;
    struct Label_page *lpg;
    int r = kobject_get_page(&l->lb_ko, slotnum / NUM_LB_ENT_PER_PAGE,
			     (void **) &lpg, page_excl_dirty);
    if (r < 0)
	return r;

    *slotp = &lpg->lp_ent[slotnum % NUM_LB_ENT_PER_PAGE];
    return 0;
}

static int
label_contains(const struct Label *l, uint64_t cat)
{
    if (!l)
	return 0;

    for (uint32_t i = 0; i < l->lb_nent; i++) {
	const uint64_t *entp;
	assert(0 == label_get_slotp_read(l, i, &entp));
	if (*entp == cat)
	    return 1;
    }

    return 0;
}

int
label_alloc(struct Label **lp, label_type t)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_label, 0, 0, 0, &ko);
    if (r < 0)
	return r;

    struct Label *l = &ko->lb;
    l->lb_type = t;
    l->lb_nent = 0;
    memset(&l->lb_ent[0], 0, sizeof(l->lb_ent));

    *lp = l;
    return 0;
}

int
label_copy(const struct Label *src, struct Label **dstp)
{
    struct Label *dst;
    int r = label_alloc(&dst, src->lb_type);
    if (r < 0)
	return r;

    dst->lb_nent = src->lb_nent;
    memcpy(&dst->lb_ent[0], &src->lb_ent[0],
	   NUM_LB_ENT_INLINE * sizeof(dst->lb_ent[0]));

    r = kobject_copy_pages(&src->lb_ko, &dst->lb_ko);
    if (r < 0)
	return r;

    *dstp = dst;
    return 0;
}

int
label_add(struct Label *l, uint64_t cat)
{
    for (uint32_t i = 0; i < l->lb_nent; i++) {
	const uint64_t *entp;
	assert(0 == label_get_slotp_read(l, i, &entp));
	if (*entp == cat)
	    return 0;
    }

    if (l->lb_nent == label_nslots(l)) {
	int r = kobject_set_nbytes(&l->lb_ko, l->lb_ko.ko_nbytes + PGSIZE);
	if (r < 0)
	    return r;
    }

    uint64_t *entp;
    int r = label_get_slotp_write(l, l->lb_nent, &entp);
    if (r < 0)
	return r;

    assert(*entp == 0);
    *entp = cat;
    return 0;
}

int
label_to_ulabel(const struct Label *l, struct new_ulabel *ul)
{
    int r = check_user_access(ul, sizeof(*ul), SEGMAP_WRITE);
    if (r < 0)
	return r;

    uint32_t ul_size = ul->ul_size;
    uint64_t *ul_ent = ul->ul_ent;

    int mul_of = 0;
    r = check_user_access(ul_ent,
			  safe_mul64(&mul_of, ul_size, sizeof(*ul_ent)),
			  SEGMAP_WRITE);
    if (r < 0)
	return r;

    if (mul_of)
	return -E_INVAL;

    ul->ul_nent = l->lb_nent;
    if (ul_size < l->lb_nent)
	return -E_NO_SPACE;

    for (uint32_t i = 0; i < l->lb_nent; i++) {
	const uint64_t *entp;
	r = label_get_slotp_read(l, i, &entp);
	if (r < 0)
	    return r;

	ul_ent[i] = *entp;
    }

    return 0;
}

int
ulabel_to_label(struct new_ulabel *ul, struct Label **lp, label_type t)
{
    int r = check_user_access(ul, sizeof(*ul), 0);
    if (r < 0)
	return r;

    r = label_alloc(lp, t);
    if (r < 0)
	return r;

    uint32_t ul_nent = ul->ul_nent;
    uint64_t *ul_ent = ul->ul_ent;

    int mul_of = 0;
    r = check_user_access(ul_ent,
			  safe_mul64(&mul_of, ul_nent, sizeof(*ul_ent)), 0);
    if (r < 0)
	return r;

    if (mul_of)
	return -E_INVAL;

    struct Label *l = *lp;
    for (uint32_t i = 0; i < ul_nent; i++) {
	r = label_add(l, ul_ent[i]);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
label_can_flow_id(kobject_id_t a, kobject_id_t b,
		  kobject_id_t p1, kobject_id_t p2)
{
    const struct kobject *a_ko, *b_ko, *p1_ko = 0, *p2_ko = 0;
    int r;

    r = kobject_get(a, &a_ko, kobj_label, iflow_none);
    if (r < 0)
	return r;

    r = kobject_get(b, &b_ko, kobj_label, iflow_none);
    if (r < 0)
	return r;

    r = p1 ? kobject_get(p1, &p1_ko, kobj_label, iflow_none) : 0;
    if (r < 0)
	return r;

    r = p2 ? kobject_get(p2, &p2_ko, kobj_label, iflow_none) : 0;
    if (r < 0)
	return r;

    return label_can_flow(&a_ko->lb, &b_ko->lb, &p1_ko->lb, &p2_ko->lb);
}

int
label_can_flow(const struct Label *a, const struct Label *b,
	       const struct Label *p1, const struct Label *p2)
{
    assert(a && SAFE_EQUAL(a->lb_type, label_track));
    assert(b && SAFE_EQUAL(b->lb_type, label_track));

    assert(!p1 || SAFE_EQUAL(p1->lb_type, label_priv));
    assert(!p2 || SAFE_EQUAL(p2->lb_type, label_priv));

    const uint64_t *entp;
    int r;

    for (uint32_t i = 0; i < a->lb_nent; i++) {
	r = label_get_slotp_read(a, i, &entp);
	if (r < 0)
	    return r;

	if (LB_INTEGRITY(*entp))
	    continue;

	if (!label_contains(b,  *entp) &&
	    !label_contains(p1, *entp) &&
	    !label_contains(p2, *entp))
	    return -E_LABEL;
    }

    for (uint32_t i = 0; i < b->lb_nent; i++) {
	r = label_get_slotp_read(b, i, &entp);
	if (r < 0)
	    return r;

	if (LB_SECRECY(*entp))
	    continue;

	if (!label_contains(a,  *entp) &&
	    !label_contains(p1, *entp) &&
	    !label_contains(p2, *entp))
	    return -E_LABEL;
    }

    return 0;
}

int
label_subset_id(kobject_id_t a, kobject_id_t b, kobject_id_t c)
{
    const struct kobject *a_ko, *b_ko, *c_ko = 0;
    int r;

    r = kobject_get(a, &a_ko, kobj_label, iflow_none);
    if (r < 0)
	return r;

    r = kobject_get(b, &b_ko, kobj_label, iflow_none);
    if (r < 0)
	return r;

    r = c ? kobject_get(c, &c_ko, kobj_label, iflow_none) : 0;
    if (r < 0)
	return r;

    return label_subset(&a_ko->lb, &b_ko->lb, &c_ko->lb);
}

int
label_subset(const struct Label *a, const struct Label *b, const struct Label *c)
{
    assert(a  && SAFE_EQUAL(a->lb_type, label_priv));
    assert(b  && SAFE_EQUAL(b->lb_type, label_priv));
    assert(!c || SAFE_EQUAL(c->lb_type, label_priv));

    for (uint32_t i = 0; i < a->lb_nent; i++) {
	const uint64_t *entp;
	int r = label_get_slotp_read(a, i, &entp);
	if (r < 0)
	    return r;

	if (!label_contains(b, *entp) && !label_contains(c, *entp))
	    return -E_LABEL;
    }

    return 0;
}

///////////////////////////
// Label pretty-printing
///////////////////////////

void
label_cprint(const struct Label *l)
{
    cprintf("Label %p: {", l);
    for (uint32_t i = 0; i < l->lb_nent; i++) {
	const uint64_t *entp;
	assert(0 == label_get_slotp_read(l, i, &entp));
	cprintf(" %"PRIu64"(%c)", *entp, LB_SECRECY(*entp) ? 's' : 'i');
    }
    cprintf(" }\n");
}
