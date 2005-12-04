#include <machine/thread.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/handle.h>
#include <inc/error.h>

struct kobject_list ko_list;

int
kobject_get(kobject_id_t id, struct kobject **kp)
{
    LIST_FOREACH(*kp, &ko_list, ko_link)
	if ((*kp)->ko_id == id)
	    return 0;

    int r = pstate_swapin(id);
    if (r < 0)
	return r;

    return -E_RESTART;
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
    --ko->ko_ref;
}

static void
kobject_gc(struct kobject *ko)
{
    int r;

    switch (ko->ko_type) {
    case kobj_thread:
	r = thread_gc((struct Thread *) ko);
	break;

    case kobj_gate:
	r = gate_gc((struct Gate *) ko);
	break;

    case kobj_segment:
	r = segment_gc((struct Segment *) ko);
	break;

    case kobj_container:
	r = container_gc((struct Container *) ko);
	break;

    default:
	panic("kobject_free: unknown kobject type %d", ko->ko_type);
    }

    if (r == -E_RESTART)
	return;
    if (r < 0)
	cprintf("kobject_free: cannot GC type %d: %d\n", ko->ko_type, r);
    ko->ko_type = kobj_dead;
}

void
kobject_gc_scan()
{
    // Clear cur_thread to avoid putting it to sleep on behalf of
    // our swapped-in objects.
    struct Thread *t = cur_thread;
    cur_thread = 0;

    struct kobject *ko;
    LIST_FOREACH(ko, &ko_list, ko_link)
	if (ko->ko_ref == 0 && ko->ko_type != kobj_dead)
	    kobject_gc(ko);

    cur_thread = t;
}

void
kobject_swapout(struct kobject *ko)
{
    if (ko->ko_type == kobj_thread)
	thread_swapout((struct Thread *) ko);
    if (ko->ko_type == kobj_segment)
	segment_swapout((struct Segment *) ko);

    LIST_REMOVE(ko, ko_link);
    page_free(ko);
}
