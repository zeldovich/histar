#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/lib.h>

int
gate_alloc(const struct Label *l,
	   const struct Label *clearance,
	   const struct Label *verify,
	   struct Gate **gp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_gate, l, &ko);
    if (r < 0)
	return r;

    r = kobject_set_label(&ko->hdr, kolabel_clearance, clearance);
    if (r < 0)
	return r;

    r = kobject_set_label(&ko->hdr, kolabel_verify, verify);
    if (r < 0)
	return r;

    *gp = &ko->gt;
    return 0;
}
