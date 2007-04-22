#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <inc/stdio.h>
#include <string.h>
#include <inttypes.h>

int
label_grow(struct ulabel *l)
{
    uint32_t newsize = l->ul_size * 2;
    uint64_t *newent = realloc(l->ul_ent, newsize * sizeof(*l->ul_ent));
    if (newent == 0)
	return -E_NO_MEM;

    memset(&newent[l->ul_size], 0, l->ul_size * sizeof(*l->ul_ent));
    l->ul_size = newsize;
    l->ul_ent = newent;
    return 0;
}

struct ulabel *
label_alloc()
{
    struct ulabel *l = malloc(sizeof(*l));
    if (l == 0)
	return 0;

    memset(l, 0, sizeof(*l));
    l->ul_size = 16;
    l->ul_nent = 0;
    l->ul_ent = malloc(l->ul_size * sizeof(*l->ul_ent));
    if (l->ul_ent == 0) {
	free(l);
	return 0;
    }

    return l;
}

void
label_free(struct ulabel *l)
{
    free(l->ul_ent);
    free(l);
}

static int
label_find_slot(struct ulabel *l, uint64_t handle)
{
    for (uint32_t i = 0; i < l->ul_nent; i++)
	if (LB_HANDLE(l->ul_ent[i]) == handle)
	    return i;
    return -1;
}

int
label_set_level(struct ulabel *l, uint64_t handle, level_t level, int grow)
{
    int slot = label_find_slot(l, handle);
    if (slot < 0) {
	if (l->ul_nent == l->ul_size) {
	    int r = grow ? label_grow(l) : -E_NO_MEM;
	    if (r < 0)
		return r;
	}

	slot = l->ul_nent;
	l->ul_nent++;
    }

    l->ul_ent[slot] = LB_CODE(handle, level);
    return 0;
}

level_t
label_get_level(struct ulabel *l, uint64_t handle)
{
    int slot = label_find_slot(l, handle);
    if (slot < 0)
	return l->ul_default;
    else
	return LB_LEVEL(l->ul_ent[slot]);
}

static char
level_to_char(level_t lv)
{
    char lbuf[4];
    if (lv == LB_LEVEL_STAR)
	snprintf(&lbuf[0], sizeof(lbuf), "*");
    else
	snprintf(&lbuf[0], sizeof(lbuf), "%d", lv);
    return lbuf[0];
}

const char *
label_to_string(const struct ulabel *l)
{
    enum {
	nbufs = 4,
	bufsize = 1024
    };

    static char bufs[nbufs][bufsize];
    static int bufidx;

    char *buf = &bufs[bufidx][0];
    bufidx = (bufidx + 1) % nbufs;

    if (l == 0)
	return "(null)";

    uint32_t off = 0;
    off += snprintf(&buf[off], bufsize - off, "{ ");
    for (uint32_t i = 0; i < l->ul_nent; i++) {
	level_t lv = LB_LEVEL(l->ul_ent[i]);
	if (lv == l->ul_default)
	    continue;

	off += snprintf(&buf[off], bufsize - off, "%"PRIu64":%c ",
			LB_HANDLE(l->ul_ent[i]), level_to_char(lv));
    }
    off += snprintf(&buf[off], bufsize - off, "%c }",
		    level_to_char(l->ul_default));

    return buf;
}

void
label_change_star(struct ulabel *l, level_t new_level)
{
    for (uint32_t i = 0; i < l->ul_nent; i++)
	if (LB_LEVEL(l->ul_ent[i]) == LB_LEVEL_STAR)
	    l->ul_ent[i] = LB_CODE(LB_HANDLE(l->ul_ent[i]), new_level);
    if (l->ul_default == LB_LEVEL_STAR)
	l->ul_default = new_level;
}

int
label_compare(struct ulabel *a, struct ulabel *b, label_comparator cmp)
{
    for (uint32_t i = 0; i < a->ul_nent; i++) {
	uint64_t h = LB_HANDLE(a->ul_ent[i]);
	int r = cmp(label_get_level(a, h), label_get_level(b, h));
	if (r < 0)
	    return r;
    }

    for (uint32_t i = 0; i < b->ul_nent; i++) {
	uint64_t h = LB_HANDLE(b->ul_ent[i]);
	int r = cmp(label_get_level(a, h), label_get_level(b, h));
	if (r < 0)
	    return r;
    }

    int r = cmp(a->ul_default, b->ul_default);
    if (r < 0)
	return r;

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
