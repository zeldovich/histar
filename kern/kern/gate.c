#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/lib.h>

int
gate_alloc(struct Label *l, struct Gate **gp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_gate, l, &ko);
    if (r < 0)
	return r;

    struct Gate *g = &ko->u.gt;
    memset(&g->gt_target_label, 0, sizeof(g->gt_target_label));
    memset(&g->gt_te, 0, sizeof(g->gt_te));

    *gp = g;
    return 0;
}
