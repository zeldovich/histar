#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/lib.h>

int
gate_alloc(const struct Label *l, struct Gate **gp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_gate, l, &ko);
    if (r < 0)
	return r;

    *gp = &ko->u.gt;
    return 0;
}
