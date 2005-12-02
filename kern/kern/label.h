#ifndef JOS_KERN_LABEL_H
#define JOS_KERN_LABEL_H

#include <machine/types.h>
#include <machine/mmu.h>

#define NUM_LB_ENT  4

struct Label {
    uint32_t lb_num_ent;
    uint32_t lb_def_level;
    uint64_t lb_ent[NUM_LB_ENT];
};

#define LB_HANDLE(ent)		((ent) & 0x1fffffffffffffffULL)
#define LB_LEVEL(ent)		((ent) >> 61)

#define LB_LEVEL_STAR		4
#define LB_CODE(h, level)	((h) | (((uint64_t) (level)) << 61))

typedef int (level_comparator)(int, int);
level_comparator label_leq_starlo;
level_comparator label_leq_starhi;
level_comparator label_eq;

int  label_set(struct Label *l, uint64_t handle, int level);

// returns -1 if the comparator returns -1 on any relevant level pair
int  label_compare(struct Label *l1, struct Label *l2, level_comparator cmp);

#endif
