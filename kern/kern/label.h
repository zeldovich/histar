#ifndef JOS_KERN_LABEL_H
#define JOS_KERN_LABEL_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/kobjhdr.h>
#include <inc/label.h>
#include <inc/safetype.h>

typedef SAFE_TYPE(int) label_type;
#define label_track	SAFE_WRAP(label_type, 1)
#define label_priv	SAFE_WRAP(label_type, 2)

#define NUM_LB_ENT_INLINE	32
struct Label {
    struct kobject_hdr lb_ko;

    label_type lb_type;
    uint64_t lb_nent;
    uint64_t lb_ent[NUM_LB_ENT_INLINE];
};

#define NUM_LB_ENT_PER_PAGE	(PGSIZE / sizeof(uint64_t))
struct Label_page {
    uint64_t lp_ent[NUM_LB_ENT_PER_PAGE];
};

int  label_alloc(struct Label **l, label_type t)
    __attribute__ ((warn_unused_result));
int  label_copy(const struct Label *src, struct Label **dst)
    __attribute__ ((warn_unused_result));

int  label_add(struct Label *l, uint64_t category)
    __attribute__ ((warn_unused_result));

int  label_to_ulabel(const struct Label *l, struct new_ulabel *ul)
    __attribute__ ((warn_unused_result));
int  ulabel_to_label(struct new_ulabel *ul, struct Label **lp, label_type t)
    __attribute__ ((warn_unused_result));

int  label_can_flow(const struct Label *a, const struct Label *b,
		    const struct Label *p1, const struct Label *p2)
    __attribute__ ((warn_unused_result));
int  label_can_flow_id(kobject_id_t a, kobject_id_t b,
		       kobject_id_t p1, kobject_id_t p2)
    __attribute__ ((warn_unused_result));

int  label_subset(const struct Label *a, const struct Label *b, const struct Label *c)
    __attribute__ ((warn_unused_result));
int  label_subset_id(kobject_id_t a, kobject_id_t b, kobject_id_t c)
    __attribute__ ((warn_unused_result));

// debugging: print a label
void label_cprint(const struct Label *l);

#endif
