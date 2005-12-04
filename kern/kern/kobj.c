#include <machine/thread.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/handle.h>
#include <inc/error.h>

struct kobject_list ko_list;

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
    void *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct kobject *ko = p;
    memset(ko, 0, sizeof(*ko));
    ko->ko_type = type;
    ko->ko_id = handle_alloc();
    ko->ko_label = *l;

    LIST_INSERT_HEAD(&ko_list, ko, ko_link);

    *kp = ko;
    return 0;
}

void
kobject_swapin(struct kobject *ko)
{
    LIST_INSERT_HEAD(&ko_list, ko, ko_link);

    if (ko->ko_type == kobj_thread)
	thread_swapin((struct Thread *) ko);
}

void
kobject_swapin_page(struct kobject *ko, uint64_t page_num, void *p)
{
    if (ko->ko_type != kobj_segment)
	panic("kobject_swapin_page: not a segment\n");
    segment_swapin_page((struct Segment *) ko, page_num, p);
}

void *
kobject_swapout_page(struct kobject *ko, uint64_t page_num)
{
    if (ko->ko_type != kobj_segment)
	panic("kobject_swapout_page: not a segment\n");
    return segment_swapout_page((struct Segment *) ko, page_num);
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
	panic("kobject_free: unknown kobject type %d\n", ko->ko_type);
    }

    ko->ko_type = kobj_dead;
}

void
kobject_swapout(struct kobject *ko)
{
    if (ko->ko_type == kobj_thread)
	thread_swapout((struct Thread *) ko);

    LIST_REMOVE(ko, ko_link);
    page_free(ko);
}
