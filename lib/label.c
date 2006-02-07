#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>

static int
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
    l->ul_size = 4;
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

struct ulabel *
label_get_current()
{
    struct ulabel *l = label_alloc();
    int r;

retry:
    r = thread_get_label(start_env->container, l);

    if (r == -E_NO_SPACE) {
	r = label_grow(l);
	if (r == 0)
	    goto retry;
    }

    if (r < 0) {
	printf("label_get_current: %s\n", e2s(r));
	label_free(l);
	return 0;
    }

    return l;
}

int
label_set_current(struct ulabel *l)
{
    return sys_thread_set_label(l);
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
label_set_level(struct ulabel *l, uint64_t handle, level_t level, bool_t grow)
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

const char *
label_to_string(struct ulabel *l)
{
    enum {
	nbufs = 4,
	bufsize = 256
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
	char level[4];
	if (LB_LEVEL(l->ul_ent[i]) == LB_LEVEL_STAR)
	    snprintf(&level[0], 4, "*");
	else
	    snprintf(&level[0], 4, "%d", LB_LEVEL(l->ul_ent[i]));

	off += snprintf(&buf[off], bufsize - off, "%ld:%s ",
			LB_HANDLE(l->ul_ent[i]), &level[0]);
    }
    off += snprintf(&buf[off], bufsize - off, "%d }", l->ul_default);

    return buf;
}
