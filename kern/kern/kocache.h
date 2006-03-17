#ifndef JOS_KERN_KOCACHE_H
#define JOS_KERN_KOCACHE_H

#include <inc/queue.h>

struct kobject;

struct kobj_weak_ptr {
    const struct kobject *wp_kobj;
    LIST_ENTRY(kobj_weak_ptr) wp_link;
};

LIST_HEAD(kobj_weak_refs, kobj_weak_ptr);

void kobj_weak_drop(struct kobj_weak_refs *head);
void kobj_weak_put(struct kobj_weak_ptr *wp, struct kobject *ko);

#endif
