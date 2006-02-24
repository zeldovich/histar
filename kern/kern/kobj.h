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
#include <kern/pagetree.h>

#define KOBJ_DISK_SIZE	512
#define KOBJ_MEM_SIZE	1024

struct kobject_persistent {
    union {
	struct kobject_hdr hdr;
	char disk_buf[KOBJ_DISK_SIZE];

	struct Container ct;
	struct Thread th;
	struct Gate gt;
	struct Address_space as;
	struct Segment sg;
	struct Mlt mt;
    };
};

struct kobject {
    union {
	char mem_buf[KOBJ_MEM_SIZE];

	struct {
	    struct kobject_persistent;
	    struct pagetree ko_pt;
	};
    };
};

struct kobject_pair {
    struct kobject active;
    struct kobject snapshot;
};

LIST_HEAD(kobject_list, kobject_hdr);
extern struct kobject_list ko_list;

void kobject_init(void);

int  kobject_get(kobject_id_t id, const struct kobject **kpp,
		 info_flow_type iflow);
int  kobject_alloc(kobject_type_t type, const struct Label *l,
		   struct kobject **kpp);

int  kobject_set_nbytes(struct kobject_hdr *kp, uint64_t nbytes);
uint64_t
     kobject_npages(const struct kobject_hdr *kp);
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

void kobject_snapshot(struct kobject_hdr *kp);
void kobject_snapshot_release(struct kobject_hdr *kp);
struct kobject *
     kobject_get_snapshot(struct kobject_hdr *kp);
struct kobject *
     kobject_h2k(struct kobject_hdr *kh);

void kobject_incref(const struct kobject_hdr *kp);
void kobject_decref(const struct kobject_hdr *kp);

void kobject_pin_hdr(const struct kobject_hdr *kp);
void kobject_unpin_hdr(const struct kobject_hdr *kp);

void kobject_pin_page(const struct kobject_hdr *kp);
void kobject_unpin_page(const struct kobject_hdr *kp);

void kobject_negative_insert(kobject_id_t id);
void kobject_negative_remove(kobject_id_t id);
bool_t kobject_negative_contains(kobject_id_t id);

// object has to stay in-core or be brought in at startup
bool_t kobject_initial(const struct kobject *ko);

#endif
