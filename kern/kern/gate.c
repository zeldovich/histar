#include <kern/gate.h>
#include <machine/pmap.h>

int
gate_alloc(struct Gate **gp)
{
    struct Page *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct Gate *g = page2kva(p);
    memset(g, 0, sizeof(*g));

    *gp = g;
    return 0;
}

void
gate_free(struct Gate *g)
{
    if (g->gt_recv_label)
	label_free(g->gt_recv_label);
    if (g->gt_send_label)
	label_free(g->gt_send_label);
    page_free(pa2page(kva2pa(g)));
}

void
gate_decref(struct Gate *g)
{
    if (--g->gt_ref == 0)
	gate_free(g);
}
