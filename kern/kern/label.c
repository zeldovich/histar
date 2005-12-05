#include <machine/pmap.h>
#include <machine/trap.h>
#include <kern/label.h>
#include <inc/error.h>

static int
label_find_slot(struct Label *l, uint64_t handle)
{
    for (int i = 0; i < l->lb_num_ent; i++)
	if (LB_HANDLE(l->lb_ent[i]) == handle)
	    return i;

    return -E_NO_MEM;
}

static int
label_get_level(struct Label *l, uint64_t handle)
{
    int i = label_find_slot(l, handle);
    if (i < 0)
	return l->lb_def_level;
    return LB_LEVEL(l->lb_ent[i]);
}

int
label_set(struct Label *l, uint64_t handle, int level)
{
    int i = label_find_slot(l, handle);
    if (i < 0) {
	if (l->lb_num_ent == NUM_LB_ENT)
	    return -E_NO_MEM;
	i = l->lb_num_ent++;
    }

    l->lb_ent[i] = LB_CODE(handle, level);
    return 0;
}

int
label_to_ulabel(struct Label *l, struct ulabel *ul)
{
    ul = TRUP(ul);

    page_fault_mode = PFM_KILL;
    ul->ul_default = l->lb_def_level;
    ul->ul_nent = 0;
    uint32_t ul_size = ul->ul_size;
    uint64_t *ul_ent = TRUP(ul->ul_ent);
    page_fault_mode = PFM_NONE;

    for (int slot = 0; slot < l->lb_num_ent; slot++) {
	if (slot > ul_size)
	    return -E_NO_SPACE;

	page_fault_mode = PFM_KILL;
	ul_ent[slot] = l->lb_ent[slot];
	ul->ul_nent++;
	page_fault_mode = PFM_NONE;
    }

    return 0;
}

int
ulabel_to_label(struct ulabel *ul, struct Label *l)
{
    ul = TRUP(ul);

    page_fault_mode = PFM_KILL;
    l->lb_def_level = ul->ul_default;
    uint32_t ul_nent = ul->ul_nent;
    uint64_t *ul_ent = TRUP(ul->ul_ent);
    page_fault_mode = PFM_NONE;

    l->lb_num_ent = 0;
    for (int slot = 0; slot < ul_nent; slot++) {
	if (slot >= NUM_LB_ENT)
	    return -E_NO_SPACE;

	page_fault_mode = PFM_KILL;
	uint64_t ul_val = ul_ent[slot];
	page_fault_mode = PFM_NONE;

	if (LB_LEVEL(ul_val) < 0 && LB_LEVEL(ul_val) > LB_LEVEL_STAR)
	    return -E_INVAL;
	l->lb_ent[slot] = ul_val;
	l->lb_num_ent++;
    }

    return 0;
}

int
label_compare(struct Label *l1, struct Label *l2, level_comparator cmp)
{
    for (int i = 0; i < l1->lb_num_ent; i++) {
	int l1l = LB_LEVEL(l1->lb_ent[i]);
	int l2l = label_get_level(l2, LB_HANDLE(l1->lb_ent[i]));

	int r = cmp(l1l, l2l);
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < l2->lb_num_ent; i++) {
	int l1l = label_get_level(l1, LB_HANDLE(l2->lb_ent[i]));
	int l2l = LB_LEVEL(l2->lb_ent[i]);

	int r = cmp(l1l, l2l);
	if (r < 0)
	    return r;
    }

    int l1d = l1->lb_def_level;
    int l2d = l2->lb_def_level;

    int r = cmp(l1d, l2d);
    if (r < 0)
	return r;

    return 0;
}

int
label_leq_starlo(int a, int b)
{
    if (a == LB_LEVEL_STAR)
	return 0;
    if (b == LB_LEVEL_STAR)
	return -E_LABEL;
    if (a <= b)
	return 0;
    return -E_LABEL;
}

int
label_leq_starhi(int a, int b)
{
    if (b == LB_LEVEL_STAR)
	return 0;
    if (a == LB_LEVEL_STAR)
	return -E_LABEL;
    if (a <= b)
	return 0;
    return -E_LABEL;
}

int
label_eq(int a, int b)
{
    if (a == b)
	return 0;
    return -E_LABEL;
}
