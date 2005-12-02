#include <machine/thread.h>
#include <kern/gate.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/label.h>
#include <kern/unique.h>
#include <inc/error.h>

static struct Container_list container_list;

int
container_alloc(struct Container **cp)
{
    struct Page *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct Container *c = page2kva(p);
    for (int i = 0; i < NUM_CT_OBJ; i++)
	c->ct_obj[i].type = cobj_none;
    c->ct_hdr.idx = unique_alloc();

    LIST_INSERT_HEAD(&container_list, c, ct_hdr.link);
    *cp = c;

    return 0;
}

static void
container_addref(container_object_type type, void *ptr)
{
    switch (type) {
    case cobj_thread:
	((struct Thread *) ptr)->th_ref++;
	break;

    case cobj_gate:
	((struct Gate*) ptr)->gt_ref++;
	break;

    case cobj_container:
	break;

    case cobj_segment:
	segment_addref(ptr);
	break;

    default:
	panic("container_addref unknown type %d", type);
    }
}

int
container_put(struct Container *c, container_object_type type, void *ptr)
{
    if (cur_thread) {
	int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_eq);
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_CT_OBJ; i++) {
	if (c->ct_obj[i].type == cobj_none) {
	    c->ct_obj[i].type = type;
	    c->ct_obj[i].ptr = ptr;
	    container_addref(type, ptr);
	    return i;
	}
    }

    return -E_NO_MEM;
}

int
container_get(struct Container *c, uint64_t slot,
	      container_object_type type,
	      struct container_object **cop)
{
    if (cur_thread) {
	int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_leq_starhi);
	if (r < 0)
	    return r;
    }

    if (slot >= NUM_CT_OBJ)
	return -E_INVAL;
    if (type != cobj_any && type != c->ct_obj[slot].type)
	return -E_INVAL;

    *cop = &c->ct_obj[slot];
    return 0;
}

int
container_unref(struct Container *c, uint64_t slot)
{
    if (cur_thread) {
	int r = label_compare(c->ct_hdr.label, cur_thread->th_label, label_eq);
	if (r < 0)
	    return r;
    }

    struct container_object *cobj;
    int r = container_get(c, slot, cobj_any, &cobj);
    if (r < 0)
	return r;

    if (cobj == 0)
	return 0;

    switch (cobj->type) {
    case cobj_none:
	break;

    case cobj_thread:
	thread_decref(cobj->ptr);
	break;

    case cobj_gate:
	gate_decref(cobj->ptr);
	break;

    case cobj_segment:
	segment_decref(cobj->ptr);
	break;

    case cobj_container:
	// XXX user-controllable recursion depth
	container_free(cobj->ptr);
	break;

    default:
	panic("unknown container object type %d", cobj->type);
    }

    cobj->type = cobj_none;
    return 0;
}

void
container_free(struct Container *c)
{
    LIST_REMOVE(c, ct_hdr.link);

    for (int i = 0; i < NUM_CT_OBJ; i++)
	container_unref(c, i);

    if (c->ct_hdr.label)
	label_free(c->ct_hdr.label);

    struct Page *p = pa2page(kva2pa(c));
    page_free(p);
}

int
container_find(struct Container **cp, uint64_t cidx)
{
    LIST_FOREACH(*cp, &container_list, ct_hdr.link)
	if ((*cp)->ct_hdr.idx == cidx)
	    return 0;

    return -E_INVAL;
}

int
cobj_get(struct cobj_ref ref, container_object_type type, void *storep)
{
    struct Container *c;
    int r = container_find(&c, ref.container);
    if (r < 0)
	return r;

    struct container_object *co;
    r = container_get(c, ref.slot, type, &co);
    if (r < 0)
	return r;

    *(void**)storep = co->ptr;
    return 0;
}
