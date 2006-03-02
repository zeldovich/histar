#ifndef JOS_KERN_LABEL_H
#define JOS_KERN_LABEL_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <inc/label.h>

#define NUM_LB_ENT	8

struct Label {
    level_t lb_def_level;
    uint64_t lb_ent[NUM_LB_ENT];
};

#define LB_ENT_EMPTY		(~(0UL))

struct level_comparator_buf;
typedef struct level_comparator_buf *level_comparator;
extern level_comparator label_leq_starlo;
extern level_comparator label_leq_starhi;
extern level_comparator label_leq_starok;
extern level_comparator label_eq;
extern level_comparator label_leq_starhi_rhs_0_except_star;

void label_init(struct Label *l, level_t def);
int  label_set(struct Label *l, uint64_t handle, level_t level)
    __attribute__ ((warn_unused_result));

// user label handling
int  label_to_ulabel(const struct Label *l, struct ulabel *ul)
    __attribute__ ((warn_unused_result));
int  ulabel_to_label(struct ulabel *ul, struct Label *l)
    __attribute__ ((warn_unused_result));

// returns an error if the comparator errors out on any relevant level pair
int  label_compare(const struct Label *l1,
		   const struct Label *l2, level_comparator cmp)
    __attribute__ ((warn_unused_result));

// computes max according to specified level ordering (<= operator)
int  label_max(const struct Label *a, const struct Label *b,
	       struct Label *dst, level_comparator leq)
    __attribute__ ((warn_unused_result));

// debugging: print a label
void label_cprint(const struct Label *l);

#endif
