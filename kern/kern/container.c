#include <kern/container.h>
#include <kern/unique.h>
#include <machine/pmap.h>
#include <machine/thread.h>
#include <inc/error.h>
#include <kern/gate.h>
#include <kern/segment.h>

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

    case cobj_pmap:
	page_map_addref(ptr);
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

struct container_object *
container_get(struct Container *c, uint32_t slot)
{
    if (slot >= NUM_CT_OBJ)
	return 0;

    return &c->ct_obj[slot];
}

void
container_unref(struct Container *c, uint32_t slot)
{
    struct container_object *cobj = container_get(c, slot);
    if (cobj == 0)
	return;

    switch (cobj->type) {
    case cobj_none:
	break;

    case cobj_thread:
	thread_decref(cobj->ptr);
	break;

    case cobj_pmap:
	page_map_decref(cobj->ptr);
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

struct Container *
container_find(uint64_t cidx)
{
    struct Container *c;
    LIST_FOREACH(c, &container_list, ct_hdr.link)
	if (c->ct_hdr.idx == cidx)
	    return c;
    return 0;
}
