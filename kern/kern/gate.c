#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/lib.h>

int
gate_alloc(const struct Label *l,
	   const struct Label *ownership,
	   const struct Label *clearance,
	   const struct Label *guard,
	   struct Gate **gp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_gate, l, ownership, clearance, &ko);
    if (r < 0)
	return r;

    r = kobject_set_label(&ko->hdr, kolabel_verify_ownership, guard);
    if (r < 0)
	return r;

    *gp = &ko->gt;
    return 0;
}
