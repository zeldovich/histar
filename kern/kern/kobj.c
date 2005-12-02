#include <machine/thread.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/unique.h>
#include <inc/error.h>

static struct kobject_list ko_list;

int
kobject_get(kobject_id_t id, struct kobject **kp)
{
    LIST_FOREACH(*kp, &ko_list, ko_link)
	if ((*kp)->ko_id == id)
	    return 0;
    return -E_INVAL;
}

int
kobject_alloc(kobject_type_t type, struct Label *l, struct kobject **kp)
{
    struct Page *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct kobject *ko = page2kva(p);
    ko->ko_type = type;
    ko->ko_id = unique_alloc();
    ko->ko_ref = 0;

    r = label_copy(l, &ko->ko_label);
    if (r < 0) {
	page_free(p);
	return r;
    }

    LIST_INSERT_HEAD(&ko_list, ko, ko_link);

    *kp = ko;
    return 0;
}

void
kobject_incref(struct kobject *ko)
{
    ++ko->ko_ref;
}

void
kobject_decref(struct kobject *ko)
{
    if (--ko->ko_ref == 0)
	kobject_free(ko);
}

void
kobject_free(struct kobject *ko)
{
    LIST_REMOVE(ko, ko_link);
    label_free(ko->ko_label);

    switch (ko->ko_type) {
    case kobj_thread:
	thread_gc((struct Thread *) ko);
	break;

    case kobj_gate:
	gate_gc((struct Gate *) ko);
	break;

    case kobj_segment:
	segment_gc((struct Segment *) ko);
	break;

    case kobj_container:
	container_gc((struct Container *) ko);
	break;

    default:
	panic("Unknown kobject type %d\n", ko->ko_type);
    }

    page_free(pa2page(kva2pa(ko)));
}
