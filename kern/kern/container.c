#include <kern/container.h>
#include <kern/unique.h>
#include <machine/pmap.h>
#include <machine/thread.h>

static struct Container_list container_list;

int
container_alloc(struct Container **cp)
{
    struct Page *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct Container *c = page2kva(p);
    int i;
    for (i = 0; i < NUM_CT_OBJ; i++)
	c->ct_obj[i].type = cobj_none;
    c->ct_hdr.idx = unique_alloc();;

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

    case cobj_container:
	break;

    default:
	panic("container_addref unknown type %d", type);
    }
}

int
container_put(struct Container *c, container_object_type type, void *ptr)
{
    int i;
    for (i = 0; i < NUM_CT_OBJ; i++) {
	if (c->ct_obj[i].type == cobj_none) {
	    c->ct_obj[i].type = type;
	    c->ct_obj[i].ptr = ptr;
	    container_addref(type, ptr);
	    return i;
	}
    }

    panic("out of space in container");
}

void
container_unref(struct Container *c, int idx)
{
    switch (c->ct_obj[idx].type) {
    case cobj_thread:
	thread_decref(c->ct_obj[idx].ptr);
	break;

    case cobj_container:
	// XXX user-controllable recursion depth
	container_free(c->ct_obj[idx].ptr);
	break;

    default:
	panic("unknown container object type %d", c->ct_obj[idx].type);
    }
}

void
container_free(struct Container *c)
{
    LIST_REMOVE(c, ct_hdr.link);

    int i;
    for (i = 0; i < NUM_CT_OBJ; i++)
	if (c->ct_obj[i].type != cobj_none)
	    container_unref(c, i);

    struct Page *p = pa2page(kva2pa(c));
    page_free(p);
}
