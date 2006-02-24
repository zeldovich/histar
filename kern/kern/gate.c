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

    ko->gt.gt_clearance = *clearance;
    *gp = &ko->gt;
    return 0;
}
