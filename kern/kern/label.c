#include <machine/pmap.h>
#include <machine/trap.h>
#include <kern/label.h>
#include <inc/error.h>

static int
label_find_slot(struct Label *l, uint64_t handle)
{
    for (int i = 0; i < NUM_LB_ENT; i++)
	if (l->lb_ent[i] != LB_ENT_EMPTY && LB_HANDLE(l->lb_ent[i]) == handle)
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

void
label_init(struct Label *l)
{
    for (int i = 0; i < NUM_LB_ENT; i++)
	l->lb_ent[i] = LB_ENT_EMPTY;
}

int
label_set(struct Label *l, uint64_t handle, int level)
{
    int i = label_find_slot(l, handle);
    if (i < 0) {
	if (level == l->lb_def_level)
	    return 0;

	for (i = 0; i < NUM_LB_ENT; i++)
	    if (l->lb_ent[i] == LB_ENT_EMPTY)
		break;

	if (i == NUM_LB_ENT)
	    return -E_NO_MEM;
    }

    l->lb_ent[i] = (level == l->lb_def_level) ? LB_ENT_EMPTY
					      : LB_CODE(handle, level);
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

    int slot = 0;
    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	if (slot >= ul_size)
	    return -E_NO_SPACE;

	page_fault_mode = PFM_KILL;
	ul_ent[slot] = l->lb_ent[i];
	slot++;
	ul->ul_nent++;
	page_fault_mode = PFM_NONE;
    }

    return 0;
}

int
ulabel_to_label(struct ulabel *ul, struct Label *l)
{
    ul = TRUP(ul);

    label_init(l);
    page_fault_mode = PFM_KILL;
    l->lb_def_level = ul->ul_default;
    uint32_t ul_nent = ul->ul_nent;
    uint64_t *ul_ent = TRUP(ul->ul_ent);
    page_fault_mode = PFM_NONE;

    // XXX minor annoyance if ul_nent is huge
    for (int i = 0; i < ul_nent; i++) {
	page_fault_mode = PFM_KILL;
	uint64_t ul_val = ul_ent[i];
	page_fault_mode = PFM_NONE;

	int level = LB_LEVEL(ul_val);
	if (level < 0 && level > LB_LEVEL_STAR)
	    return -E_INVAL;

	int r = label_set(l, LB_HANDLE(ul_val), level);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
label_compare(struct Label *l1, struct Label *l2, level_comparator cmp)
{
    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l1->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(l1->lb_ent[i]);
	int r = cmp(label_get_level(l1, h), label_get_level(l2, h));
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l2->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(l2->lb_ent[i]);
	int r = cmp(label_get_level(l1, h), label_get_level(l2, h));
	if (r < 0)
	    return r;
    }

    int r = cmp(l1->lb_def_level, l2->lb_def_level);
    if (r < 0)
	return r;

    return 0;
}

static int
level_max(int a, int b, level_comparator leq)
{
    return (leq(a, b) >= 0) ? b : a;
}

int
label_max(struct Label *a, struct Label *b, struct Label *dst, level_comparator leq)
{
    dst->lb_def_level = level_max(a->lb_def_level, b->lb_def_level, leq);

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (a->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(a->lb_ent[i]);
	int r = label_set(dst, h, level_max(label_get_level(a, h),
					    label_get_level(b, h), leq));
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (b->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(b->lb_ent[i]);
	int r = label_set(dst, h, level_max(label_get_level(a, h),
					    label_get_level(b, h), leq));
	if (r < 0)
	    return r;
    }

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
