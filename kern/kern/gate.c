#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/lib.h>

int
gate_alloc(const struct Label *l,
	   const struct Label *clearance,
	   struct Gate **gp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_gate, l, &ko);
    if (r < 0)
	return r;

    ko->gt.gt_clearance_id = clearance->lb_ko.ko_id;
    kobject_incref(&clearance->lb_ko);

    *gp = &ko->gt;
    return 0;
}

int
gate_gc(struct Gate *g)
{
    const struct kobject *clearance;
    int r = kobject_get(g->gt_clearance_id, &clearance, kobj_label, iflow_none);
    if (r < 0)
	return r;

    kobject_decref(&clearance->hdr);
    return 0;
}
