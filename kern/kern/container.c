#include <kern/container.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/lib.h>
#include <inc/error.h>

int
container_alloc(struct Label *l, struct Container **cp)
{
    struct Container *c;
    int r = kobject_alloc(kobj_container, l, (struct kobject **)&c);
    if (r < 0)
	return r;

    *cp = c;
    return 0;
}

static int
container_slot_get(struct Container *c, uint64_t slotn, kobject_id_t **slotp)
{
    int ko_page = 0;
    while (slotn > NUM_CT_OBJ_PER_PAGE) {
	ko_page++;
	slotn -= NUM_CT_OBJ_PER_PAGE;
    }

    struct container_page *cpg;
    int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg);
    if (r < 0)
	return r;

    *slotp = &cpg->ct_obj[slotn];
    return 0;
}

static int
container_slot_alloc(struct Container *c, kobject_id_t **slotp)
{
    struct container_page *cpg;
    int r;

    for (int ko_page = 0; ko_page < c->ct_ko.ko_npages; ko_page++) {
	r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg);
	if (r < 0)
	    return r;

	for (int i = 0; i < NUM_CT_OBJ_PER_PAGE; i++) {
	    if (cpg->ct_obj[i] == kobject_id_null) {
		*slotp = &cpg->ct_obj[i];
		return ko_page * NUM_CT_OBJ_PER_PAGE + i;
	    }
	}
    }

    r = kobject_set_npages(&c->ct_ko, c->ct_ko.ko_npages + 1);
    if (r < 0)
	return r;

    int newpage = c->ct_ko.ko_npages - 1;
    r = kobject_get_page(&c->ct_ko, newpage, (void**)&cpg);
    if (r < 0)
	panic("container_slot_alloc: cannot get newly-allocated page");

    for (int i = 0; i < NUM_CT_OBJ_PER_PAGE; i++)
	cpg->ct_obj[i] = kobject_id_null;

    *slotp = &cpg->ct_obj[0];
    return newpage * NUM_CT_OBJ_PER_PAGE;
}

int
container_put(struct Container *c, struct kobject *ko)
{
    kobject_id_t *slotp;
    int slot = container_slot_alloc(c, &slotp);
    if (slot < 0)
	return slot;

    *slotp = ko->ko_id;
    kobject_incref(ko);
    return slot;
}

int
container_unref(struct Container *c, uint64_t slot)
{
    kobject_id_t *slotp;
    int r = container_slot_get(c, slot, &slotp);
    if (r < 0)
	return r;

    kobject_id_t id = *slotp;
    if (id == kobject_id_null)
	return 0;

    struct kobject *ko;
    r = kobject_get(id, &ko, iflow_none);
    if (r < 0)
	return r;

    kobject_decref(ko);
    *slotp = kobject_id_null;
    return 0;
}

int
container_nslots(struct Container *c)
{
    return c->ct_ko.ko_npages * NUM_CT_OBJ_PER_PAGE;
}

int
container_gc(struct Container *c)
{
    int nslots = container_nslots(c);
    for (int i = 0; i < nslots; i++) {
	int r = container_unref(c, i);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
container_find(struct Container **cp, kobject_id_t id, info_flow_type iflow)
{
    int r = kobject_get(id, (struct kobject **)cp, iflow);
    if (r < 0)
	return r;
    if ((*cp)->ct_ko.ko_type != kobj_container)
	return -E_INVAL;
    return 0;
}

int
cobj_get(struct cobj_ref ref, kobject_type_t type, struct kobject **storep, info_flow_type iflow)
{
    struct Container *c;
    int r = container_find(&c, ref.container, iflow_read);
    if (r < 0)
	return r;

    kobject_id_t *slotp;
    r = container_slot_get(c, ref.slot, &slotp);
    if (r < 0)
	return r;

    if (*slotp == kobject_id_null)
	return -E_NOT_FOUND;

    struct kobject *ko;
    r = kobject_get(*slotp, &ko, iflow);
    if (r < 0)
	return r;

    if (type != kobj_any && type != ko->ko_type)
	return -E_INVAL;

    *storep = ko;
    return 0;
}
