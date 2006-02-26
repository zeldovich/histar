#include <machine/pmap.h>
#include <machine/trap.h>
#include <kern/label.h>
#include <inc/error.h>

static int
label_find_slot(const struct Label *l, uint64_t handle)
{
    for (int i = 0; i < NUM_LB_ENT; i++)
	if (l->lb_ent[i] != LB_ENT_EMPTY && LB_HANDLE(l->lb_ent[i]) == handle)
	    return i;

    return -E_NO_MEM;
}

static int
label_get_level(const struct Label *l, uint64_t handle)
{
    int i = label_find_slot(l, handle);
    if (i < 0)
	return l->lb_def_level;
    return LB_LEVEL(l->lb_ent[i]);
}

void
label_init(struct Label *l, level_t def)
{
    l->lb_def_level = def;
    for (int i = 0; i < NUM_LB_ENT; i++)
	l->lb_ent[i] = LB_ENT_EMPTY;
}

int
label_set(struct Label *l, uint64_t handle, level_t level)
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
label_to_ulabel(const struct Label *l, struct ulabel *ul)
{
    int r = page_user_incore((void**) &ul, sizeof(*ul));
    if (r < 0)
	return r;

    ul->ul_default = l->lb_def_level;
    ul->ul_nent = 0;
    uint32_t ul_size = ul->ul_size;
    uint64_t *ul_ent = ul->ul_ent;

    r = page_user_incore((void**) &ul_ent, ul_size * sizeof(*ul_ent));
    if (r < 0)
	return r;

    uint32_t slot = 0;
    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	if (slot >= ul_size)
	    return -E_NO_SPACE;

	ul_ent[slot] = l->lb_ent[i];
	slot++;
	ul->ul_nent++;
    }

    return 0;
}

int
ulabel_to_label(struct ulabel *ul, struct Label *l)
{
    int r = page_user_incore((void**) &ul, sizeof(*ul));
    if (r < 0)
	return r;

    label_init(l, ul->ul_default);
    uint32_t ul_nent = ul->ul_nent;
    uint64_t *ul_ent = ul->ul_ent;

    r = page_user_incore((void**) &ul_ent, ul_nent * sizeof(*ul_ent));
    if (r < 0)
	return r;

    // XXX minor annoyance if ul_nent is huge
    for (uint32_t i = 0; i < ul_nent; i++) {
	uint64_t ul_val = ul_ent[i];

	int level = LB_LEVEL(ul_val);
	if (level < 0 && level > LB_LEVEL_STAR)
	    return -E_INVAL;

	r = label_set(l, LB_HANDLE(ul_val), level);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
label_compare(const struct Label *l1,
	      const struct Label *l2, level_comparator cmp)
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
label_max(const struct Label *a, const struct Label *b,
	  struct Label *dst, level_comparator leq)
{
    label_init(dst, level_max(a->lb_def_level, b->lb_def_level, leq));

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
label_leq_starlo(level_t a, level_t b)
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
label_leq_starhi(level_t a, level_t b)
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
label_eq(level_t a, level_t b)
{
    if (a == b)
	return 0;
    return -E_LABEL;
}

int
label_leq_starhi_rhs_0_except_star(level_t a, level_t b)
{
    return label_leq_starhi(a, b == LB_LEVEL_STAR ? LB_LEVEL_STAR : 0);
}

void
label_cprint(const struct Label *l)
{
    cprintf("Label %p: {", l);
    for (int i = 0; i < NUM_LB_ENT; i++) {
	uint64_t ent = l->lb_ent[i];
	if (ent != LB_ENT_EMPTY) {
	    level_t level = LB_LEVEL(ent);
	    char lchar[2];
	    if (level == LB_LEVEL_STAR)
		lchar[0] = '*';
	    else
		snprintf(&lchar[0], 2, "%d", level);
	    cprintf(" %lu:%c,", LB_HANDLE(ent), lchar[0]);
	}
    }
    cprintf(" %d }\n", l->lb_def_level);
}
