#include <inc/lib.h>
#include <inc/taint.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inttypes.h>

enum { taint_debug = 0 };

// Copy the writable pieces of the address space
enum {
    taint_cow_label_ents = 32,
    taint_as_maxsize = 128,
};

static void
taint_cow_cprint_label(struct new_ulabel *l)
{
    cprintf("{ ");
    for (uint32_t i = 0; i < l->ul_nent; i++) {
	uint64_t c = l->ul_ent[i];
	cprintf("%"PRIu64"(%c) ", c, LB_SECRECY(c) ? 's' : 'i');
    }
    cprintf("}");
}

static void
taint_cow_compute_label(struct new_ulabel *th_label,
			struct new_ulabel *th_owner,
			struct new_ulabel *obj_label)
{
    for (uint32_t i = 0; i < obj_label->ul_nent; i++) {
	uint64_t c = obj_label->ul_ent[i];
	if (!label_contains(th_owner, c))
	    obj_label->ul_ent[i] = 0;
    }

    for (uint32_t i = 0; i < th_label->ul_nent; i++) {
	uint64_t c = th_label->ul_ent[i];
	assert(0 == label_add(obj_label, c, 0));
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
    // if we can write to the address space, that's "good enough"
    int r = sys_obj_write(cur_as, 0, 0, 0);
    if (r != -E_LABEL)
	return 0;

    char namebuf[KOBJ_NAME_LEN];
    uint64_t th_ents[taint_cow_label_ents];
    uint64_t own_ents[taint_cow_label_ents];
    uint64_t obj_ents[taint_cow_label_ents];

    struct new_ulabel th_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &th_ents[0] };
    struct new_ulabel th_owner =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &own_ents[0] };
    struct new_ulabel obj_label =
	{ .ul_size = taint_cow_label_ents, .ul_ent = &obj_ents[0] };

    struct cobj_ref cur_th = COBJ(0, sys_self_id());
    ERRCHECK(sys_obj_get_label(cur_th, &th_label));
    ERRCHECK(sys_obj_get_ownership(cur_th, &th_owner));

    if (taint_debug) {
	cprintf("taint_cow: thread label ");
	taint_cow_cprint_label(&th_label);
	cprintf("\n");
    }

    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    ERRCHECK(sys_obj_get_label(COBJ(start_env_ro->proc_container,
				    start_env_ro->proc_container), &obj_label));

    taint_cow_compute_label(&th_label, &th_owner, &obj_label);
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

    // Compute label of new address space
    ERRCHECK(sys_obj_get_label(cur_as, &obj_label));
    taint_cow_compute_label(&th_label, &th_owner, &obj_label);

    ERRCHECK(sys_obj_get_name(cur_as, &namebuf[0]));
    int64_t id = sys_as_copy(cur_as, mlt_ct, &obj_label, &namebuf[0]);
    if (id < 0)
	panic("taint_cow: cannot copy as: %s", e2s(id));

    struct cobj_ref new_as = COBJ(mlt_ct, id);
    if (taint_debug) {
	cprintf("taint_cow: new as: %"PRIu64".%"PRIu64", label: ", new_as.container, new_as.object);
	taint_cow_cprint_label(&obj_label);
	cprintf("\n");
    }

    /*
     * Structure to map original segment IDs to copied segment IDs.
     */
    uint64_t segment_copies[taint_as_maxsize][2];
    for (uint32_t i = 0; i < taint_as_maxsize; i++)
	segment_copies[i][0] = segment_copies[i][1] = 0;
    uint32_t next_copyidx = 0;

    for (uint32_t i = 0; ; i++) {
	struct u_segment_mapping usm;
	usm.kslot = i;

	r = sys_as_get_slot(cur_as, &usm);
	if (r == -E_INVAL)
	    break;
	if (r < 0)
	    panic("taint_cow: cannot get AS slot: %s", e2s(r));

	if (taint_debug) {
	    cprintf("taint_cow: mapping of %"PRIu64".%"PRIu64" at VA %p--%p, flags 0x%x\n",
		    usm.segment.container, usm.segment.object,
		    usm.va, usm.va + usm.num_pages * PGSIZE, usm.flags);
	}

	if (!(usm.flags & SEGMAP_WRITE) || usm.segment.container != cur_as.container)
	    continue;

	int already_copied = 0;
	for (uint32_t j = 0; j < next_copyidx; j++)
	    if (usm.segment.object == segment_copies[j][0])
		already_copied = 1;

	if (already_copied)
	    continue;

	r = sys_obj_write(usm.segment, 0, 0, 0);
	if (r != -E_LABEL)
	    continue;

	ERRCHECK(sys_obj_get_label(usm.segment, &obj_label));
	taint_cow_compute_label(&th_label, &th_owner, &obj_label);

	if (taint_debug) {
	    cprintf("taint_cow: trying to copy segment %"PRIu64".%"PRIu64", VA %p, label ",
		    usm.segment.container,
		    usm.segment.object,
		    usm.va);
	    taint_cow_cprint_label(&obj_label);
	    cprintf("\n");
	}

	ERRCHECK(sys_obj_get_name(usm.segment, &namebuf[0]));
	id = sys_segment_copy(usm.segment, mlt_ct,
			      &obj_label, &namebuf[0]);
	if (id < 0)
	    panic("taint_cow: cannot copy segment: %s", e2s(id));

	if (next_copyidx >= taint_as_maxsize)
	    panic("taint_cow: too many AS entries");

	segment_copies[next_copyidx][0] = usm.segment.object;
	segment_copies[next_copyidx][1] = id;
	next_copyidx++;
    }

    /*
     * Second pass to re-map all of the segments we copied above.
     */
    for (uint32_t i = 0; ; i++) {
	struct u_segment_mapping usm;
	usm.kslot = i;

	r = sys_as_get_slot(cur_as, &usm);
	if (r == -E_INVAL)
	    break;
	if (r < 0)
	    panic("taint_cow: cannot get AS slot: %s", e2s(r));

	if (!(usm.flags & SEGMAP_READ) || usm.segment.container == mlt_ct)
	    continue;

	for (uint32_t j = 0; j < next_copyidx; j++) {
	    if (usm.segment.object == segment_copies[j][0]) {
		usm.segment.container = mlt_ct;
		usm.segment.object = segment_copies[j][1];
		break;
	    }
	}

	if (usm.segment.container == mlt_ct)
	    ERRCHECK(sys_as_set_slot(new_as, &usm));
    }

    ERRCHECK(sys_self_set_as(new_as));
    segment_as_switched();
    segment_as_invalidate_nowb(0);

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
	cprintf("taint_cow: trying to COW; shared/proc CT %"PRIu64"/%"PRIu64"\n",
		start_env_ro->shared_container, start_env_ro->proc_container);
    if (start_env_ro->taint_cow_as.object) {
	cur_as = start_env_ro->taint_cow_as;
	if (taint_debug)
	    cprintf("taint_cow: using checkpointed AS %"PRIu64".%"PRIu64"\n",
		    cur_as.container, cur_as.object);
    }
    int r = taint_cow_slow(cur_as, taint_container, declassify_gate);
    utrap_set_mask(maskold);
    return r;
}
