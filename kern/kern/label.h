#ifndef JOS_KERN_LABEL_H
#define JOS_KERN_LABEL_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <inc/label.h>

typedef unsigned char level_t;

#define NUM_LB_ENT	8

struct Label {
    level_t lb_def_level;
    uint64_t lb_ent[NUM_LB_ENT];
};

#define LB_ENT_EMPTY		(~(0L))

typedef int (level_comparator)(int, int);
level_comparator label_leq_starlo;
level_comparator label_leq_starhi;
level_comparator label_eq;

void label_init(struct Label *l, level_t def);
int  label_set(struct Label *l, uint64_t handle, level_t level);

// user label handling
int  label_to_ulabel(const struct Label *l, struct ulabel *ul);
int  ulabel_to_label(struct ulabel *ul, struct Label *l);

// returns an error if the comparator errors out on any relevant level pair
int  label_compare(const struct Label *l1,
		   const struct Label *l2, level_comparator cmp);

// computes max according to specified level ordering (<= operator)
int  label_max(const struct Label *a, const struct Label *b,
	       struct Label *dst, level_comparator cmp);

#endif
