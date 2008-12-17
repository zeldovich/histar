#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <inc/stdio.h>
#include <string.h>
#include <inttypes.h>

int
label_grow(struct new_ulabel *l)
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

struct new_ulabel *
label_alloc()
{
    struct new_ulabel *l = malloc(sizeof(*l));
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
label_free(struct new_ulabel *l)
{
    free(l->ul_ent);
    free(l);
}

int
label_add(struct new_ulabel *l, uint64_t cat, int grow)
{
    if (label_contains(l, cat))
	return 0;

    for (uint32_t i = 0; i < l->ul_nent; i++) {
	if (l->ul_ent[i] == 0) {
	    l->ul_ent[i] = cat;
	    return 0;
	}
    }

    if (l->ul_nent == l->ul_size) {
	int r = grow ? label_grow(l) : -E_NO_MEM;
	if (r < 0)
	    return r;
    }

    l->ul_ent[l->ul_nent] = cat;
    l->ul_nent++;
    return 0;
}

int
label_contains(struct new_ulabel *l, uint64_t cat)
{
    for (uint32_t i = 0; i < l->ul_nent; i++)
	if (l->ul_ent[i] == cat)
	    return 1;
    return 0;
}

const char *
label_to_string(const struct new_ulabel *l)
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
    for (uint32_t i = 0; i < l->ul_nent; i++)
	off += snprintf(&buf[off], bufsize - off, "%"PRIu64"(%c) ",
			l->ul_ent[i], LB_SECRECY(l->ul_ent[i]) ? 's' : 'i');
    off += snprintf(&buf[off], bufsize - off, "}");

    return buf;
}
