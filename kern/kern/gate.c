#include <kern/gate.h>
#include <machine/pmap.h>

int
gate_alloc(struct Label *l, struct Gate **gp)
{
    struct Gate *g;
    int r = kobject_alloc(kobj_gate, l, (struct kobject **)&g);
    if (r < 0)
	return r;

    memset(&g->gt_target_label, 0, sizeof(g->gt_target_label));
    memset(&g->gt_te, 0, sizeof(g->gt_te));

    *gp = g;
    return 0;
}

void
gate_gc(struct Gate *g)
{
}
