#include <kern/container.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/lib.h>
#include <inc/error.h>

int
container_alloc(const struct Label *l, struct Container **cp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_container, l, &ko);
    if (r < 0)
	return r;

    struct Container *c = &ko->ct;
    *cp = c;
    return 0;
}

static int
container_slot_get(const struct Container *c, uint64_t slotn,
		   struct container_slot **csp, page_sharing_mode rw)
{
    if (slotn < NUM_CT_SLOT_INLINE) {
	if (SAFE_EQUAL(rw, page_excl_dirty))
	    kobject_dirty(&c->ct_ko);
	*csp = (struct container_slot *) &c->ct_slots[slotn];
	return 0;
    }
    slotn -= NUM_CT_SLOT_INLINE;

    uint64_t ko_page = slotn / NUM_CT_SLOT_PER_PAGE;
    uint64_t pg_slot = slotn % NUM_CT_SLOT_PER_PAGE;

    struct container_page *cpg;
    int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
    if (r < 0)
	return r;

    *csp = &cpg->cpg_slot[pg_slot];
    return 0;
}

static int
container_slot_find(const struct Container *c, kobject_id_t ko_id,
		    struct container_slot **csp, page_sharing_mode rw)
{
    for (uint32_t i = 0; i < NUM_CT_SLOT_INLINE; i++) {
	const struct container_slot *cs = &c->ct_slots[i];
	if (cs->cs_id == ko_id && cs->cs_ref > 0) {
	    if (csp)
		*csp = (struct container_slot *) cs;
	    if (SAFE_EQUAL(rw, page_excl_dirty))
		kobject_dirty(&c->ct_ko);
	    return 0;
	}
    }

    for (uint64_t ko_page = 0; ko_page < kobject_npages(&c->ct_ko); ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->cpg_slot[i];
	    if (cs->cs_id == ko_id && cs->cs_ref > 0) {
		if (csp)
		    *csp = cs;
		return 0;
	    }
	}
    }

    return -E_NOT_FOUND;
}

static int
container_slot_alloc(struct Container *c, struct container_slot **csp)
{
    for (uint32_t i = 0; i < NUM_CT_SLOT_INLINE; i++) {
	struct container_slot *cs = &c->ct_slots[i];
	if (cs->cs_ref == 0) {
	    *csp = cs;
	    return 0;
	}
    }

    for (uint64_t ko_page = 0; ko_page < kobject_npages(&c->ct_ko); ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, page_excl_dirty);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->cpg_slot[i];
	    if (cs->cs_ref == 0) {
		*csp = cs;
		return 0;
	    }
	}
    }

    uint64_t newpage = kobject_npages(&c->ct_ko);
    int r = kobject_set_nbytes(&c->ct_ko, (newpage + 1) * PGSIZE);
    if (r < 0)
	return r;

    struct container_page *cpg;
    r = kobject_get_page(&c->ct_ko, newpage, (void**)&cpg, page_excl_dirty);
    if (r < 0)
	panic("container_slot_alloc: cannot get newly-allocated page");

    for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++)
	cpg->cpg_slot[i].cs_ref = 0;

    *csp = &cpg->cpg_slot[0];
    return 0;
}

static int
container_slot_addref(struct Container *c, struct container_slot *cs,
		      const struct kobject_hdr *ko)
{
    int r = kobject_incref(ko, &c->ct_ko);
    if (r < 0)
	return r;

    cs->cs_id = ko->ko_id;
    cs->cs_ref++;
    return 0;
}

static void
container_slot_decref(struct Container *c, struct container_slot *cs,
		      const struct kobject_hdr *ko)
{
    cs->cs_ref--;
    kobject_decref(ko, &c->ct_ko);
}

int
container_put(struct Container *c, const struct kobject_hdr *ko)
{
    assert(ko->ko_type < kobj_ntypes);
    if ((c->ct_avoid_types & (1 << ko->ko_type)))
	return -E_INVAL;

    struct container_slot *cs;
    int r = container_slot_find(c, ko->ko_id, &cs, page_excl_dirty);
    if (r == -E_NOT_FOUND)
	r = container_slot_alloc(c, &cs);
    if (r < 0)
	return r;

    if (ko->ko_ref > 0 && !(ko->ko_flags & KOBJ_FIXED_QUOTA))
	return -E_VAR_QUOTA;

    r = container_slot_addref(c, cs, ko);
    if (r < 0)
	return r;

    return 0;
}

int
container_get(const struct Container *c, kobject_id_t *idp, uint64_t slot)
{
    struct container_slot *cs;
    int r = container_slot_get(c, slot, &cs, page_shared_ro);
    if (r < 0)
	return r;
    if (cs->cs_ref == 0)
	return -E_NOT_FOUND;

    *idp = cs->cs_id;
    return 0;
}

int
container_unref(struct Container *c, const struct kobject_hdr *ko)
{
    struct container_slot *cs;
    int r = container_slot_find(c, ko->ko_id, &cs, page_excl_dirty);
    if (r < 0)
	return r;

    container_slot_decref(c, cs, ko);
    return 0;
}

uint64_t
container_nslots(const struct Container *c)
{
    return NUM_CT_SLOT_INLINE +
	   kobject_npages(&c->ct_ko) * NUM_CT_SLOT_PER_PAGE;
}

int
container_gc(struct Container *c)
{
    uint64_t nslots = container_nslots(c);
    for (uint64_t i = 0; i < nslots; i++) {
	struct container_slot *cs;
	int r = container_slot_get(c, i, &cs, page_excl_dirty);
	if (r < 0)
	    return r;

	while (cs->cs_ref > 0) {
	    const struct kobject *ko;
	    r = kobject_get(cs->cs_id, &ko, kobj_any, iflow_none);
	    if (r < 0)
		return r;

	    container_slot_decref(c, cs, &ko->hdr);
	}
    }

    return 0;
}

int
container_find(const struct Container **cp, kobject_id_t id, info_flow_type iflow)
{
    const struct kobject *ko;
    int r = kobject_get(id, &ko, kobj_container, iflow);
    if (r < 0)
	return r;

    *cp = &ko->ct;
    return 0;
}

int
cobj_get(struct cobj_ref ref, kobject_type_t type,
	 const struct kobject **storep, info_flow_type iflow)
{
    // Some objects can be named without a container -- the current
    // thread and the thread-local segment.
    bool_t global_object =
	(ref.container == 0) &&
	((ref.object == kobject_id_thread_sg) ||
	 (cur_thread && ref.object == cur_thread->th_ko.ko_id));

    if (!global_object) {
	const struct Container *c;
	int r = container_find(&c, ref.container, iflow_read);
	if (r < 0)
	    return r;

	// Every container "contains" itself
	if (ref.object != c->ct_ko.ko_id) {
	    r = container_slot_find(c, ref.object, 0, page_shared_ro);
	    if (r < 0)
		return r;
	}
    }

    return kobject_get(ref.object, storep, type, iflow);
}

int
container_has(const struct Container *c, kobject_id_t id)
{
    return container_slot_find(c, id, 0, page_shared_ro);
}
