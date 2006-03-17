#include <machine/types.h>
#include <kern/kocache.h>
#include <kern/kobj.h>

void
kobj_weak_put(struct kobj_weak_ptr *wp,
	      struct kobject *ko)
{
    if (wp->wp_kobj)
	LIST_REMOVE(wp, wp_link);

    wp->wp_kobj = ko;
    LIST_INSERT_HEAD(&ko->ko_weak_refs, wp, wp_link);
}

void
kobj_weak_drop(struct kobj_weak_refs *head)
{
    struct kobj_weak_ptr *wp, *next;
    for (wp = LIST_FIRST(head); wp; wp = next) {
	next = LIST_NEXT(wp, wp_link);
	LIST_REMOVE(wp, wp_link);
	wp->wp_kobj = 0;
    }
}
