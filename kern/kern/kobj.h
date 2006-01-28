#ifndef JOS_KERN_KOBJ_H
#define JOS_KERN_KOBJ_H

#include <kern/kobjhdr.h>

#include <machine/thread.h>
#include <machine/as.h>
#include <machine/memlayout.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/gate.h>
#include <kern/mlt.h>

#define KOBJ_SIZE	(PGSIZE / 2)

struct kobject {
    union {
	struct kobject_hdr hdr;
	char buf[KOBJ_SIZE];

	struct Container ct;
	struct Thread th;
	struct Gate gt;
	struct Address_space as;
	struct Segment sg;
	struct Mlt mt;
    } u;
};

struct kobject_pair {
    struct kobject active;
    struct kobject snapshot;
};

LIST_HEAD(kobject_list, kobject_hdr);
extern struct kobject_list ko_list;

int  kobject_get(kobject_id_t id, const struct kobject **kpp,
		 info_flow_type iflow);
int  kobject_alloc(kobject_type_t type, struct Label *l, struct kobject **kpp);

int  kobject_set_npages(struct kobject_hdr *kp, uint64_t npages);
int  kobject_get_page(const struct kobject_hdr *kp, uint64_t page_num,
		      void **pp, page_rw_mode rw);

// Mark the kobject as dirty and return the same kobject
struct kobject *
     kobject_dirty(const struct kobject_hdr *kh);

// The object has been brought in from disk.
void kobject_swapin(struct kobject *kp);

// Called when the object has been written out to disk and the
// in-memory copy should be discarded.
void kobject_swapout(struct kobject *kp);

// Called by the timer interrupt to garbage-collect free'd kobjects
void kobject_gc_scan(void);

void kobject_snapshot(struct kobject_hdr *kp);
void kobject_snapshot_release(struct kobject_hdr *kp);
struct kobject *
     kobject_get_snapshot(struct kobject_hdr *kp);
struct kobject *
     kobject_h2k(struct kobject_hdr *kh);

void kobject_incref(const struct kobject_hdr *kp);
void kobject_decref(const struct kobject_hdr *kp);

void kobject_incpin(const struct kobject_hdr *kp);
void kobject_decpin(const struct kobject_hdr *kp);

#endif
