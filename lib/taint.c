#include <inc/lib.h>
#include <inc/taint.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/mlt.h>

enum {
    taint_debug = 0,
};

// Copy the writable pieces of the address space
enum {
    taint_cow_label_ents = 64,
    taint_cow_as_ents = 16,
};

static void
taint_cow_compute_label(struct ulabel *cur_label, struct ulabel *obj_label)
{
    for (uint32_t j = 0; j < cur_label->ul_nent; j++) {
	uint64_t h = LB_HANDLE(cur_label->ul_ent[j]);
	level_t obj_level = label_get_level(obj_label, h);
	level_t cur_level = label_get_level(cur_label, h);
	if (cur_level == LB_LEVEL_STAR)
	    continue;
	if (obj_level == LB_LEVEL_STAR || obj_level < cur_level)
	    assert(0 == label_set_level(obj_label, h, cur_level, 0));
    }
}

void
taint_cow(void)
{
    uint64_t cur_ents[taint_cow_label_ents];
    uint64_t obj_ents[taint_cow_label_ents];

    struct ulabel cur_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &cur_ents[0] };
    struct ulabel obj_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &obj_ents[0] };

    int r = thread_get_label(&cur_label);
    if (r < 0)
	panic("taint_cow: thread_get_label: %s", e2s(r));

    struct cobj_ref cur_as;
    r = sys_thread_get_as(&cur_as);
    if (r < 0)
	panic("taint_cow: sys_thread_get_as: %s", e2s(r));

    r = sys_obj_get_label(cur_as, &obj_label);
    if (r < 0)
	panic("taint_cow: cannot get as label: %s", e2s(r));

    // if we can write to the address space, that's "good enough"
    r = label_compare(&cur_label, &obj_label, label_leq_starlo);
    if (r == 0) {
	if (taint_debug)
	    printf("taint_cow: no need to cow\n");
	return;
    }

    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    r = sys_obj_get_label(COBJ(start_env_ro->container, start_env_ro->container), &obj_label);
    if (r < 0)
	panic("taint_cow: cannot get parent container label: %s", e2s(r));

    taint_cow_compute_label(&cur_label, &obj_label);

    struct cobj_ref mlt = start_env_ro->taint_mlt;
    uint8_t buf[MLT_BUF_SIZE];
    uint64_t mlt_ct;
    r = sys_mlt_put(mlt, &obj_label, &buf[0], &mlt_ct);
    if (r < 0)
	panic("taint_cow: cannot store garbage in MLT: %s", e2s(r));

    struct u_segment_mapping uas_ents[taint_cow_as_ents];
    struct u_address_space uas =
	{ .size = taint_cow_as_ents, .ents = &uas_ents[0] };
    r = sys_as_get(cur_as, &uas);
    if (r < 0)
	panic("taint_cow: sys_as_get: %s", e2s(r));

    for (uint32_t i = 0; i < uas.nent; i++) {
	if (!(uas.ents[i].flags & SEGMAP_WRITE))
	    continue;

	r = sys_obj_get_label(uas.ents[i].segment, &obj_label);
	if (r < 0)
	    panic("taint_cow: cannot get label: %s", e2s(r));

	r = label_compare(&cur_label, &obj_label, label_leq_starlo);
	if (r == 0)
	    continue;

	if (taint_debug)
	    cprintf("taint_cow: trying to copy segment %ld.%ld\n",
		    uas.ents[i].segment.container,
		    uas.ents[i].segment.object);

	taint_cow_compute_label(&cur_label, &obj_label);

	char namebuf[KOBJ_NAME_LEN];
	r = sys_obj_get_name(uas.ents[i].segment, &namebuf[0]);
	if (r < 0)
	    panic("taint_cow: cannot get segment name: %s", e2s(r));

	int64_t id = sys_segment_copy(uas.ents[i].segment, mlt_ct,
				      &obj_label, &namebuf[0]);
	if (id < 0)
	    panic("taint_cow: cannot copy segment: %s", e2s(id));

	uas.ents[i].segment = COBJ(mlt_ct, id);
    }

    r = sys_obj_get_label(cur_as, &obj_label);
    if (r < 0)
	panic("taint_cow: cannot get as label again: %s", e2s(r));

    taint_cow_compute_label(&cur_label, &obj_label);

    int64_t id = sys_as_create(mlt_ct, &obj_label, "taint cow as");
    if (id < 0)
	panic("taint_cow: cannot create new as: %s", e2s(id));

    struct cobj_ref new_as = COBJ(mlt_ct, id);
    r = sys_as_set(new_as, &uas);
    if (r < 0)
	panic("taint_cow: cannot populate new as: %s", e2s(r));

    r = sys_thread_set_as(new_as);
    if (r < 0)
	panic("taint_cow: cannot switch to new as: %s", e2s(r));

    if (taint_debug)
	cprintf("taint_cow: new as: %lu.%lu\n", new_as.container, new_as.object);

    struct ulabel *l_seg = segment_get_default_label();
    if (l_seg) {
	taint_cow_compute_label(&cur_label, l_seg);
	segment_set_default_label(l_seg);
    }

    start_env->container = mlt_ct;
}
