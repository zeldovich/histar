#include <kern/container.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/lib.h>
#include <inc/error.h>

int
container_alloc(struct Label *l, struct Container **cp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_container, l, &ko);
    if (r < 0)
	return r;

    struct Container *c = &ko->u.ct;
    *cp = c;
    return 0;
}

static int
container_slot_get(const struct Container *c, uint64_t slotn,
		   struct container_slot **csp, kobj_rw_mode rw)
{
    uint64_t ko_page = 0;
    while (slotn > NUM_CT_SLOT_PER_PAGE) {
	ko_page++;
	slotn -= NUM_CT_SLOT_PER_PAGE;
    }

    struct container_page *cpg;
    int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
    if (r < 0)
	return r;

    *csp = &cpg->ct_slot[slotn];
    return 0;
}

static int
container_slot_find(const struct Container *c, kobject_id_t ko_id,
		    struct container_slot **csp, kobj_rw_mode rw)
{
    for (uint64_t ko_page = 0; ko_page < c->ct_ko.ko_npages; ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->ct_slot[i];
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
    for (uint64_t ko_page = 0; ko_page < c->ct_ko.ko_npages; ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, kobj_rw);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->ct_slot[i];
	    if (cs->cs_ref == 0) {
		*csp = cs;
		return 0;
	    }
	}
    }

    uint64_t newpage = c->ct_ko.ko_npages;
    int r = kobject_set_npages(&c->ct_ko, newpage + 1);
    if (r < 0)
	return r;

    struct container_page *cpg;
    r = kobject_get_page(&c->ct_ko, newpage, (void**)&cpg, kobj_rw);
    if (r < 0)
	panic("container_slot_alloc: cannot get newly-allocated page");

    for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++)
	cpg->ct_slot[i].cs_ref = 0;

    *csp = &cpg->ct_slot[0];
    return 0;
}

int
container_put(struct Container *c, struct kobject_hdr *ko)
{
    struct container_slot *cs;
    int r = container_slot_find(c, ko->ko_id, &cs, kobj_rw);
    if (r == -E_NOT_FOUND)
	r = container_slot_alloc(c, &cs);
    if (r < 0)
	return r;

    cs->cs_id = ko->ko_id;
    cs->cs_ref++;
    kobject_incref(ko);
    return 0;
}

int
container_get(const struct Container *c, kobject_id_t *idp, uint64_t slot)
{
    struct container_slot *cs;
    int r = container_slot_get(c, slot, &cs, kobj_ro);
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
    int r = container_slot_find(c, ko->ko_id, &cs, kobj_rw);
    if (r < 0)
	return r;

    cs->cs_ref--;
    kobject_decref(ko);
    return 0;
}

uint64_t
container_nslots(const struct Container *c)
{
    return c->ct_ko.ko_npages * NUM_CT_SLOT_PER_PAGE;
}

int
container_gc(struct Container *c)
{
    uint64_t nslots = container_nslots(c);
    for (uint64_t i = 0; i < nslots; i++) {
	struct container_slot *cs;
	int r = container_slot_get(c, i, &cs, kobj_rw);
	if (r < 0)
	    return r;

	while (cs->cs_ref > 0) {
	    const struct kobject *ko;
	    r = kobject_get(cs->cs_id, &ko, iflow_none);
	    if (r < 0)
		return r;

	    cs->cs_ref--;
	    kobject_decref(&kobject_dirty(&ko->u.hdr)->u.hdr);
	}
    }

    return 0;
}

int
container_find(const struct Container **cp, kobject_id_t id, info_flow_type iflow)
{
    const struct kobject *ko;
    int r = kobject_get(id, &ko, iflow);
    if (r < 0)
	return r;

    if (ko->u.hdr.ko_type != kobj_container)
	return -E_INVAL;

    *cp = &ko->u.ct;
    return 0;
}

int
cobj_get(struct cobj_ref ref, kobject_type_t type,
	 const struct kobject **storep, info_flow_type iflow)
{
    const struct Container *c;
    int r = container_find(&c, ref.container, iflow_read);
    if (r < 0)
	return r;

    r = container_slot_find(c, ref.object, 0, kobj_ro);
    if (r < 0)
	return r;

    r = kobject_get(ref.object, storep, iflow);
    if (r < 0)
	return r;

    if (type != kobj_any && type != (*storep)->u.hdr.ko_type)
	return -E_INVAL;

    return 0;
}
