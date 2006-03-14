#include <machine/pmap.h>
#include <machine/trap.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <inc/error.h>

////////////////////////////////
// Level comparison functions
////////////////////////////////

typedef int (level_comparator_fn) (level_t, level_t);
struct level_comparator_buf {
    level_comparator_fn *gen;
    int inited;
    int8_t cmp[LB_LEVEL_STAR + 1][LB_LEVEL_STAR + 1];
    int8_t max[LB_LEVEL_STAR + 1][LB_LEVEL_STAR + 1];
};

static int
label_leq_starlo_fn(level_t a, level_t b)
{
    if (a == LB_LEVEL_STAR)
	return 0;
    if (b == LB_LEVEL_STAR)
	return -E_LABEL;
    return (a <= b) ? 0 : -E_LABEL;
}

static int
label_leq_starhi_fn(level_t a, level_t b)
{
    if (b == LB_LEVEL_STAR)
	return 0;
    if (a == LB_LEVEL_STAR)
	return -E_LABEL;
    return (a <= b) ? 0 : -E_LABEL;
}

static int
label_leq_starok_fn(level_t a, level_t b)
{
    if (a == LB_LEVEL_STAR || b == LB_LEVEL_STAR)
	return 0;
    return (a <= b) ? 0 : -E_LABEL;
}

static int
label_eq_fn(level_t a, level_t b)
{
    return (a == b) ? 0 : -E_LABEL;
}

static int
label_leq_starhi_rhs_0_except_star_fn(level_t a, level_t b)
{
    return label_leq_starhi_fn(a, b == LB_LEVEL_STAR ? LB_LEVEL_STAR : 0);
}

#define LEVEL_COMPARATOR(x)						\
    static struct level_comparator_buf x##_buf = { .gen = &x##_fn };	\
    level_comparator x = &x##_buf

LEVEL_COMPARATOR(label_leq_starok);
LEVEL_COMPARATOR(label_leq_starlo);
LEVEL_COMPARATOR(label_leq_starhi);
LEVEL_COMPARATOR(label_eq);
LEVEL_COMPARATOR(label_leq_starhi_rhs_0_except_star);

static void
level_comparator_init(level_comparator c)
{
    if (c->inited)
	return;

    for (int a = 0; a <= LB_LEVEL_STAR; a++) {
	for (int b = 0; b <= LB_LEVEL_STAR; b++) {
	    c->cmp[a][b] = c->gen(a, b);
	    c->max[a][b] = (c->cmp[a][b] == 0) ? b : a;
	}
    }
    c->inited = 1;
}

////////////////////
// Label handling
////////////////////

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

int
label_alloc(struct Label **lp, level_t def)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_label, 0, &ko);
    if (r < 0)
	return r;

    struct Label *l = &ko->lb;
    l->lb_def_level = def;
    for (int i = 0; i < NUM_LB_ENT; i++)
	l->lb_ent[i] = LB_ENT_EMPTY;

    *lp = l;
    return 0;
}

int
label_copy(const struct Label *src, struct Label **dstp)
{
    struct Label *dst;
    int r = label_alloc(&dst, LB_LEVEL_UNDEF);
    if (r < 0)
	return r;

    dst->lb_def_level = src->lb_def_level;
    for (int i = 0; i < NUM_LB_ENT; i++)
	dst->lb_ent[i] = src->lb_ent[i];

    *dstp = dst;
    return 0;
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
    int r = check_user_access(ul, sizeof(*ul), SEGMAP_WRITE);
    if (r < 0)
	return r;

    ul->ul_default = l->lb_def_level;
    ul->ul_nent = 0;
    uint32_t ul_size = ul->ul_size;
    uint64_t *ul_ent = ul->ul_ent;

    r = check_user_access(ul_ent, ul_size * sizeof(*ul_ent), SEGMAP_WRITE);
    if (r < 0)
	return r;

    uint32_t slot = 0;
    uint32_t overflow = 0;
    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	if (slot < ul_size) {
	    ul_ent[slot] = l->lb_ent[i];
	    slot++;
	} else {
	    overflow++;
	}
    }

    ul->ul_nent = slot;
    ul->ul_needed = overflow;

    return overflow ? -E_NO_SPACE : 0;
}

int
ulabel_to_label(struct ulabel *ul, struct Label *l)
{
    int r = check_user_access(ul, sizeof(*ul), 0);
    if (r < 0)
	return r;

    l->lb_def_level = ul->ul_default;
    uint32_t ul_nent = ul->ul_nent;
    uint64_t *ul_ent = ul->ul_ent;

    r = check_user_access(ul_ent, ul_nent * sizeof(*ul_ent), 0);
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
    assert(l1);
    assert(l2);

    level_comparator_init(cmp);

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l1->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(l1->lb_ent[i]);
	level_t lv1 = LB_LEVEL(l1->lb_ent[i]);
	int r = cmp->cmp[lv1][label_get_level(l2, h)];
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (l2->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(l2->lb_ent[i]);
	level_t lv2 = LB_LEVEL(l2->lb_ent[i]);
	int r = cmp->cmp[label_get_level(l1, h)][lv2];
	if (r < 0)
	    return r;
    }

    int r = cmp->cmp[l1->lb_def_level][l2->lb_def_level];
    if (r < 0)
	return r;

    return 0;
}

int
label_max(const struct Label *a, const struct Label *b,
	  struct Label *dst, level_comparator leq)
{
    level_comparator_init(leq);
    dst->lb_def_level = leq->max[a->lb_def_level][b->lb_def_level];

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (a->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(a->lb_ent[i]);
	level_t alv = LB_LEVEL(a->lb_ent[i]);
	int r = label_set(dst, h, leq->max[alv][label_get_level(b, h)]);
	if (r < 0)
	    return r;
    }

    for (int i = 0; i < NUM_LB_ENT; i++) {
	if (b->lb_ent[i] == LB_ENT_EMPTY)
	    continue;

	uint64_t h = LB_HANDLE(b->lb_ent[i]);
	level_t blv = LB_LEVEL(b->lb_ent[i]);
	int r = label_set(dst, h, leq->max[label_get_level(a, h)][blv]);
	if (r < 0)
	    return r;
    }

    return 0;
}

///////////////////////////
// Label pretty-printing
///////////////////////////

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
