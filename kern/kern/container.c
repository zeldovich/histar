#include <machine/thread.h>
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

    for (int i = 0; i < NUM_CT_OBJ; i++)
	c->ct_obj[i] = kobject_id_null;

    *cp = c;
    return 0;
}

int
container_put(struct Container *c, struct kobject *ko)
{
    if (cur_thread) {
	int r = label_compare(c->ct_ko.ko_label, cur_thread->th_ko.ko_label, label_eq);
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_CT_OBJ; i++) {
	if (c->ct_obj[i] == kobject_id_null) {
	    c->ct_obj[i] = ko->ko_id;
	    kobject_incref(ko);
	    return i;
	}
    }

    return -E_NO_MEM;
}

int
container_unref(struct Container *c, uint64_t slot)
{
    if (cur_thread) {
	int r = label_compare(c->ct_ko.ko_label, cur_thread->th_ko.ko_label, label_eq);
	if (r < 0)
	    return r;
    }

    if (slot >= NUM_CT_OBJ)
	return -E_INVAL;

    kobject_id_t id = c->ct_obj[slot];
    if (id == kobject_id_null)
	return 0;

    struct kobject *ko;
    int r = kobject_get(id, &ko);
    if (r < 0)
	return r;

    // XXX user-controllable recursion depth, if ko->ko_type==kobj_container
    kobject_decref(ko);
    c->ct_obj[slot] = kobject_id_null;
    return 0;
}

void
container_gc(struct Container *c)
{
    for (int i = 0; i < NUM_CT_OBJ; i++) {
	int r = container_unref(c, i);
	// XXX no error-handling path
	if (r < 0)
	    cprintf("container_gc(): cannot unref slot %d: %d\n", i, r);
    }
}

int
container_find(struct Container **cp, kobject_id_t id)
{
    int r = kobject_get(id, (struct kobject **)cp);
    if (r < 0)
	return r;
    if ((*cp)->ct_ko.ko_type != kobj_container)
	return -E_INVAL;
    return 0;
}

int
cobj_get(struct cobj_ref ref, kobject_type_t type, struct kobject **storep)
{
    struct Container *c;
    int r = container_find(&c, ref.container);
    if (r < 0)
	return r;

    if (cur_thread) {
	r = label_compare(c->ct_ko.ko_label, cur_thread->th_ko.ko_label, label_leq_starhi);
	if (r < 0)
	    return r;
    }

    if (ref.slot >= NUM_CT_OBJ)
	return -E_INVAL;

    if (c->ct_obj[ref.slot] == kobject_id_null)
	return -E_NOT_FOUND;

    struct kobject *ko;
    r = kobject_get(c->ct_obj[ref.slot], &ko);
    if (r < 0)
	return r;

    if (type != kobj_any && type != ko->ko_type)
	return -E_INVAL;

    *storep = ko;
    return 0;
}
