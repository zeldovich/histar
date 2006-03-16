#include <inc/lib.h>
#include <inc/taint.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/stdio.h>

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

#define ERRCHECK(e) \
    do {					\
	int64_t __r = e;			\
	if (__r < 0)				\
	    panic("%s: %s", #e, e2s(__r));	\
    } while (0)

void
taint_cow(uint64_t taint_container)
{
    uint64_t cur_ents[taint_cow_label_ents];
    uint64_t obj_ents[taint_cow_label_ents];

    struct ulabel cur_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &cur_ents[0] };
    struct ulabel obj_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &obj_ents[0] };

    struct cobj_ref cur_as;
    ERRCHECK(sys_self_get_as(&cur_as));
    ERRCHECK(sys_obj_get_label(cur_as, &obj_label));
    ERRCHECK(thread_get_label(&cur_label));

    // if we can write to the address space, that's "good enough"
    int r = label_compare(&cur_label, &obj_label, label_leq_starlo);
    if (r == 0) {
	if (taint_debug)
	    cprintf("taint_cow: no need to cow\n");
	return;
    }

    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    ERRCHECK(sys_obj_get_label(COBJ(start_env_ro->proc_container,
				    start_env_ro->proc_container), &obj_label));

    taint_cow_compute_label(&cur_label, &obj_label);

    int64_t mlt_ct_id = sys_container_alloc(taint_container, &obj_label,
					    "taint_cow container");
    ERRCHECK(mlt_ct_id);

    // To placate gcc which is obsessed with signedness
    uint64_t mlt_ct = mlt_ct_id;

    struct u_segment_mapping uas_ents[taint_cow_as_ents];
    struct u_address_space uas =
	{ .size = taint_cow_as_ents, .ents = &uas_ents[0] };
    ERRCHECK(sys_as_get(cur_as, &uas));

    for (uint32_t i = 0; i < uas.nent; i++) {
	if (!(uas.ents[i].flags & SEGMAP_WRITE))
	    continue;
	if (uas.ents[i].segment.container == mlt_ct)
	    continue;

	ERRCHECK(sys_obj_get_label(uas.ents[i].segment, &obj_label));

	r = label_compare(&cur_label, &obj_label, label_leq_starlo);
	if (r == 0)
	    continue;

	if (taint_debug)
	    cprintf("taint_cow: trying to copy segment %ld.%ld\n",
		    uas.ents[i].segment.container,
		    uas.ents[i].segment.object);

	taint_cow_compute_label(&cur_label, &obj_label);

	char namebuf[KOBJ_NAME_LEN];
	ERRCHECK(sys_obj_get_name(uas.ents[i].segment, &namebuf[0]));

	int64_t id = sys_segment_copy(uas.ents[i].segment, mlt_ct,
				      &obj_label, &namebuf[0]);
	if (id < 0)
	    panic("taint_cow: cannot copy segment: %s", e2s(id));

	uint64_t old_id = uas.ents[i].segment.object;
	for (uint32_t j = 0; j < uas.nent; j++)
	    if (uas.ents[j].segment.object == old_id)
		uas.ents[j].segment = COBJ(mlt_ct, id);
    }

    ERRCHECK(sys_obj_get_label(cur_as, &obj_label));

    taint_cow_compute_label(&cur_label, &obj_label);

    int64_t id = sys_as_create(mlt_ct, &obj_label, "taint cow as");
    if (id < 0)
	panic("taint_cow: cannot create new as: %s", e2s(id));

    struct cobj_ref new_as = COBJ(mlt_ct, id);
    ERRCHECK(sys_as_set(new_as, &uas));
    ERRCHECK(sys_self_set_as(new_as));

    if (taint_debug)
	cprintf("taint_cow: new as: %lu.%lu\n", new_as.container, new_as.object);

    start_env->proc_container = mlt_ct;

    // XXX we probably won't be able to the old shared_container
    // anymore, so might as well use the new container for anything
    // that would have gone there..
    start_env->shared_container = mlt_ct;
}
