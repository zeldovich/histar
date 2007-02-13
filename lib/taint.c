#include <inc/lib.h>
#include <inc/taint.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/utrap.h>

enum { taint_debug = 0 };

// Copy the writable pieces of the address space
enum {
    taint_cow_label_ents = 32,
    taint_cow_as_ents = 32,
};

static void
taint_cow_cprint_label(struct ulabel *l)
{
    level_t def = l->ul_default;
    cprintf("{ ");
    for (uint32_t i = 0; i < l->ul_nent; i++) {
	uint64_t h = LB_HANDLE(l->ul_ent[i]);
	level_t lv = label_get_level(l, h);
	if (lv != def) {
	    if (lv == LB_LEVEL_STAR)
		cprintf("%ld:*, ", h);
	    else
		cprintf("%ld:%d, ", h, lv);
	}
    }
    cprintf("%d }", def);
}

static void
taint_cow_compute_label(struct ulabel *cur_label, struct ulabel *obj_label)
{
    for (uint32_t i = 0; i < cur_label->ul_nent; i++) {
	uint64_t h = LB_HANDLE(cur_label->ul_ent[i]);
	level_t obj_level = label_get_level(obj_label, h);
	level_t cur_level = label_get_level(cur_label, h);
	if (cur_level == LB_LEVEL_STAR)
	    continue;
	if (obj_level < cur_level)
	    assert(0 == label_set_level(obj_label, h, cur_level, 0));
    }

    for (uint32_t i = 0; i < obj_label->ul_nent; i++) {
	uint64_t h = LB_HANDLE(obj_label->ul_ent[i]);
	level_t obj_level = label_get_level(obj_label, h);
	level_t cur_level = label_get_level(cur_label, h);
	if (cur_level == LB_LEVEL_STAR)
	    continue;
	if (obj_level < cur_level)
	    assert(0 == label_set_level(obj_label, h, cur_level, 0));
    }
}

#define ERRCHECK(e)						\
    do {							\
	int64_t __r = e;					\
	if (__r < 0) {						\
	    cprintf("taint_cow[%s:%d]: %s: %s\n",		\
		    __FILE__, __LINE__, #e, e2s(__r));		\
	    sys_self_halt();					\
	}							\
    } while (0)

static int __attribute__((noinline))
taint_cow_fastcheck(struct cobj_ref cur_as)
{
    struct u_segment_mapping usm;
    usm.kslot = ~0U;

    /*
     * If we can write to the current address space, that's good enough.
     */
    int r = sys_as_set_slot(cur_as, &usm);
    if (r == -E_LABEL)
	return -1;
    if (r == -E_INVAL)		/* kslot out of range */
	return 0;

    cprintf("taint_cow_fastcheck: odd return value %s\n", e2s(r));
    return -1;
}

static int __attribute__((noinline))
taint_cow_slow(struct cobj_ref cur_as, uint64_t taint_container,
	       struct cobj_ref declassify_gate)
{
    char namebuf[KOBJ_NAME_LEN];
    uint64_t cur_ents[taint_cow_label_ents];
    uint64_t obj_ents[taint_cow_label_ents];

    struct ulabel cur_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &cur_ents[0] };
    struct ulabel obj_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &obj_ents[0] };

    struct cobj_ref cur_th = COBJ(0, sys_self_id());
    ERRCHECK(sys_obj_get_label(cur_as, &obj_label));
    ERRCHECK(sys_obj_get_label(cur_th, &cur_label));

    // if we can write to the address space, that's "good enough"
    int r = label_compare(&cur_label, &obj_label, label_leq_starlo);

    if (taint_debug) {
	cprintf("taint_cow: thread label ");
	taint_cow_cprint_label(&cur_label);
	cprintf("\n");
	cprintf("taint_cow: old AS label ");
	taint_cow_cprint_label(&obj_label);
	cprintf("\n");
    }

    if (r == 0) {
	if (taint_debug)
	    cprintf("taint_cow: no need to cow\n");
	return 0;
    }

    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    ERRCHECK(sys_obj_get_label(COBJ(start_env_ro->proc_container,
				    start_env_ro->proc_container), &obj_label));

    taint_cow_compute_label(&cur_label, &obj_label);
    if (taint_debug) {
	cprintf("taint_cow: new container label ");
	taint_cow_cprint_label(&obj_label);
	cprintf("\n");
    }

    int64_t mlt_ct_id = sys_container_alloc(taint_container, &obj_label,
					    "taint_cow container", 0, CT_QUOTA_INF);
    ERRCHECK(mlt_ct_id);

    // To placate gcc which is obsessed with signedness
    uint64_t mlt_ct = mlt_ct_id;

    struct u_segment_mapping uas_ents[taint_cow_as_ents];
    struct u_address_space uas =
	{ .size = taint_cow_as_ents, .ents = &uas_ents[0] };
    ERRCHECK(sys_as_get(cur_as, &uas));

    for (uint32_t i = 0; i < uas.nent; i++) {
	if (taint_debug) {
	    cprintf("taint_cow: mapping of %ld.%ld at VA %p, flags 0x%x\n",
		    uas.ents[i].segment.container, uas.ents[i].segment.object,
		    uas.ents[i].va, uas.ents[i].flags);
	}

	if (!(uas.ents[i].flags & SEGMAP_WRITE) ||
	    uas.ents[i].segment.container != cur_as.container)
	    continue;
	if (uas.ents[i].segment.container == mlt_ct)
	    continue;

	r = sys_obj_get_label(uas.ents[i].segment, &obj_label);
	if (r == -E_NOT_FOUND)
	    continue;
	ERRCHECK(r);

	r = label_compare(&cur_label, &obj_label, label_leq_starlo);
	if (r == 0)
	    continue;

	taint_cow_compute_label(&cur_label, &obj_label);

	if (taint_debug) {
	    cprintf("taint_cow: trying to copy segment %ld.%ld, VA %p, label ",
		    uas.ents[i].segment.container,
		    uas.ents[i].segment.object,
		    uas.ents[i].va);
	    taint_cow_cprint_label(&obj_label);
	    cprintf("\n");
	}

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

    ERRCHECK(sys_obj_get_name(cur_as, &namebuf[0]));
    int64_t id = sys_as_create(mlt_ct, &obj_label, &namebuf[0]);
    if (id < 0)
	panic("taint_cow: cannot create new as: %s", e2s(id));

    struct cobj_ref new_as = COBJ(mlt_ct, id);
    ERRCHECK(sys_as_set(new_as, &uas));
    ERRCHECK(sys_self_set_as(new_as));
    segment_as_switched();

    if (taint_debug) {
	cprintf("taint_cow: new as: %lu.%lu, label: ", new_as.container, new_as.object);
	taint_cow_cprint_label(&obj_label);
	cprintf("\n");
    }

    ERRCHECK(sys_self_addref(mlt_ct));
    ERRCHECK(sys_self_set_sched_parents(start_env->proc_container, mlt_ct));

    start_env->proc_container = mlt_ct;
    start_env->shared_container = taint_container;
    start_env->declassify_gate = declassify_gate;
    return 1;
}

int
taint_cow(uint64_t taint_container, struct cobj_ref declassify_gate)
{
    struct cobj_ref cur_as;
    ERRCHECK(sys_self_get_as(&cur_as));

    if (taint_cow_fastcheck(cur_as) == 0)
	return 0;

    int maskold = utrap_set_mask(1);
    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    if (taint_debug)
	cprintf("taint_cow: trying to COW; shared/proc CT %ld/%ld\n",
		start_env_ro->shared_container, start_env_ro->proc_container);
    if (start_env_ro->taint_cow_as.object) {
	cur_as = start_env_ro->taint_cow_as;
	cprintf("taint_cow: using checkpointed AS %ld.%ld\n",
		cur_as.container, cur_as.object);
    }
    int r = taint_cow_slow(cur_as, taint_container, declassify_gate);
    utrap_set_mask(maskold);
    return r;
}
