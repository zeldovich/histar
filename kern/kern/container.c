#include <kern/container.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/timer.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>

int
container_alloc(const struct Label *l, struct Container **cp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_container, l, 0, &ko);
    if (r < 0)
	return r;

    struct Container *c = &ko->ct;
    *cp = c;
    return 0;
}

static int
container_slot_get(const struct Container *c, uint64_t slotn,
		   struct container_slot **csp, page_sharing_mode rw)
{
    if (slotn < NUM_CT_SLOT_INLINE) {
	if (SAFE_EQUAL(rw, page_excl_dirty))
	    kobject_dirty(&c->ct_ko);
	*csp = (struct container_slot *) &c->ct_slots[slotn];
	return 0;
    }
    slotn -= NUM_CT_SLOT_INLINE;

    uint64_t ko_page = slotn / NUM_CT_SLOT_PER_PAGE;
    uint64_t pg_slot = slotn % NUM_CT_SLOT_PER_PAGE;

    struct container_page *cpg;
    int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
    if (r < 0)
	return r;

    *csp = &cpg->cpg_slot[pg_slot];
    return 0;
}

static int
container_slot_find(const struct Container *c, kobject_id_t ko_id,
		    struct container_slot **csp, page_sharing_mode rw)
{
    for (uint32_t i = 0; i < NUM_CT_SLOT_INLINE; i++) {
	const struct container_slot *cs = &c->ct_slots[i];
	if (cs->cs_id == ko_id && cs->cs_ref > 0) {
	    if (csp)
		*csp = (struct container_slot *) cs;
	    if (SAFE_EQUAL(rw, page_excl_dirty))
		kobject_dirty(&c->ct_ko);
	    return 0;
	}
    }

    for (uint64_t ko_page = 0; ko_page < kobject_npages(&c->ct_ko); ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, rw);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->cpg_slot[i];
	    if (cs->cs_id == ko_id && cs->cs_ref > 0) {
		if (csp)
		    *csp = cs;
		return 0;
	    }
	}
    }

    return -E_NOT_FOUND;
}

static int
container_slot_alloc(struct Container *c, struct container_slot **csp)
{
    for (uint32_t i = 0; i < NUM_CT_SLOT_INLINE; i++) {
	struct container_slot *cs = &c->ct_slots[i];
	if (cs->cs_ref == 0) {
	    *csp = cs;
	    return 0;
	}
    }

    for (uint64_t ko_page = 0; ko_page < kobject_npages(&c->ct_ko); ko_page++) {
	struct container_page *cpg;
	int r = kobject_get_page(&c->ct_ko, ko_page, (void**)&cpg, page_excl_dirty);
	if (r < 0)
	    return r;

	for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++) {
	    struct container_slot *cs = &cpg->cpg_slot[i];
	    if (cs->cs_ref == 0) {
		*csp = cs;
		return 0;
	    }
	}
    }

    uint64_t newpage = kobject_npages(&c->ct_ko);
    int r = kobject_set_nbytes(&c->ct_ko, (newpage + 1) * PGSIZE);
    if (r < 0)
	return r;

    struct container_page *cpg;
    r = kobject_get_page(&c->ct_ko, newpage, (void**)&cpg, page_excl_dirty);
    if (r < 0)
	panic("container_slot_alloc: cannot get newly-allocated page");

    for (uint32_t i = 0; i < NUM_CT_SLOT_PER_PAGE; i++)
	cpg->cpg_slot[i].cs_ref = 0;

    *csp = &cpg->cpg_slot[0];
    return 0;
}

static int
container_slot_addref(struct Container *c, struct container_slot *cs,
		      const struct kobject_hdr *ko, uint64_t extra_refs)
{
    if (cs->cs_ref == 0) {
	if (ko->ko_ref > extra_refs && !(ko->ko_flags & KOBJ_FIXED_QUOTA))
	    return -E_VAR_QUOTA;

	int r = kobject_incref(ko, &c->ct_ko);
	if (r < 0)
	    return r;

	cs->cs_id = ko->ko_id;
    }

    cs->cs_ref++;
    return 0;
}

static void
container_slot_decref(struct Container *c, struct container_slot *cs,
		      const struct kobject_hdr *ko)
{
    cs->cs_ref--;
    if (cs->cs_ref == 0)
	kobject_decref(ko, &c->ct_ko);
}

int
container_put(struct Container *c, const struct kobject_hdr *ko,
	      uint64_t extra_refs)
{
    assert(ko->ko_type < kobj_ntypes);
    if ((c->ct_avoid_types & (1 << ko->ko_type)))
	return -E_BAD_TYPE;

    struct container_slot *cs;
    int r = container_slot_find(c, ko->ko_id, &cs, page_excl_dirty);
    if (r == -E_NOT_FOUND)
	r = container_slot_alloc(c, &cs);
    if (r < 0)
	return r;

    r = container_slot_addref(c, cs, ko, extra_refs);
    if (r < 0)
	return r;

    return 0;
}

int
container_get(const struct Container *c, kobject_id_t *idp, uint64_t slot)
{
    struct container_slot *cs;
    int r = container_slot_get(c, slot, &cs, page_shared_ro);
    if (r < 0)
	return r;
    if (cs->cs_ref == 0)
	return -E_NOT_FOUND;

    *idp = cs->cs_id;
    return 0;
}

int
container_unref(struct Container *c, const struct kobject_hdr *ko, int preponly)
{
    struct container_slot *cs;
    int r = container_slot_find(c, ko->ko_id, &cs, page_excl_dirty);
    if (r < 0)
	return r;

    if (!preponly)
	container_slot_decref(c, cs, ko);
    return 0;
}

uint64_t
container_nslots(const struct Container *c)
{
    return NUM_CT_SLOT_INLINE +
	   kobject_npages(&c->ct_ko) * NUM_CT_SLOT_PER_PAGE;
}

int
container_gc(struct Container *c)
{
    uint64_t nslots = container_nslots(c);
    for (uint64_t i = 0; i < nslots; i++) {
	struct container_slot *cs;
	int r = container_slot_get(c, i, &cs, page_excl_dirty);
	if (r < 0)
	    return r;

	while (cs->cs_ref > 0) {
	    const struct kobject *ko;
	    r = kobject_get(cs->cs_id, &ko, kobj_any, iflow_none);
	    if (r < 0)
		return r;

	    container_slot_decref(c, cs, &ko->hdr);
	}
    }

    return 0;
}

int
container_find(const struct Container **cp, kobject_id_t id, info_flow_type iflow)
{
    const struct kobject *ko;
    int r = kobject_get(id, &ko, kobj_container, iflow);
    if (r < 0)
	return r;

    *cp = &ko->ct;
    return 0;
}

int
cobj_get(struct cobj_ref ref, uint8_t type,
	 const struct kobject **storep, info_flow_type iflow)
{
    // Some objects can be named without a container -- the current
    // thread and the thread-local segment.
    bool_t global_object =
	(ref.container == 0) &&
	((ref.object == kobject_id_thread_sg) ||
	 (cur_thread && ref.object == cur_thread->th_ko.ko_id));

    const struct kobject *ko;
    int r0 = kobject_get(ref.object, &ko, type, iflow);

    if (!global_object) {
	const struct Container *c;
	int r = container_find(&c, ref.container, iflow_read);
	if (r < 0)
	    return (r == -E_RESTART) ? r : -E_NOT_FOUND;

	// Every container "contains" itself
	if (ref.object == c->ct_ko.ko_id)
	    goto ct_ok;

	// Iff the last container to addref the object is ref.container,
	// and it hasn't dropped its refcount yet, ko_parent will match.
	if (r0 >= 0 && ko->hdr.ko_parent == ref.container)
	    goto ct_ok;

	r = container_slot_find(c, ref.object, 0, page_shared_ro);
	if (r < 0)
	    return r;
    }

 ct_ok:
    if (r0 >= 0)
	*storep = ko;
    return r0;
}

int
container_has(const struct Container *c, kobject_id_t id)
{
    return container_slot_find(c, id, 0, page_shared_ro);
}

int
container_has_ancestor(const struct Container *c, uint64_t ancestor)
{
 again:
    if (c->ct_ko.ko_id == ancestor)
	return 0;

    uint64_t parent_id = c->ct_ko.ko_parent;
    if (!parent_id)
	return -E_NOT_FOUND;

    const struct kobject *ko;
    int r = cobj_get(COBJ(parent_id, c->ct_ko.ko_id), kobj_container,
		     &ko, iflow_none);
    if (r < 0)
	return r;

    r = kobject_get(parent_id, &ko, kobj_container, iflow_read);
    if (r < 0)
	return r;

    c = &ko->ct;
    goto again;
}

extern const uint64_t stride1;
const struct Container *cur_ct;

void
container_pass_update(struct Container *ct, uint128_t new_global_pass)
{
    // bury this in container as well?
    uint64_t elapsed = karch_get_tsc() - ct->ct_last_update;
    ct->ct_last_update += elapsed;

    if (new_global_pass) {
        ct->ct_global_pass = new_global_pass;
    } else if (ct->ct_global_tickets) {
        ct->ct_global_pass += ((uint128_t) (stride1 / ct->ct_global_tickets)) *
                                                elapsed;
    }
}

extern uint64_t user_root_ct;
// Essentially set kobj_id in ct as runnable
void
container_join(struct Container *ct, uint64_t kobj_id)
{
    int r;
    struct container_slot *cs;
    const struct Container *temp;

recurse:
    r = container_slot_find(ct, kobj_id, &cs, page_shared_ro);
    if (r < 0)
        panic("container_join: %"PRIu64" not in this container\n", kobj_id);

    // this is for the case the same thread is in a container twice
    // this will keep from double counting it and hence nothing propagates
    // up so we are done
    if (cs->cs_runnable)
        return;


    // mark this slot as runnable and adjust the container
    cs->cs_runnable = 1;
    ct->ct_runnable += 1;

    container_pass_update(ct, 0);
    cs->cs_sched_pass = ct->ct_global_pass + cs->cs_sched_remain;
    ct->ct_global_tickets += cs->cs_sched_tickets;

    // if the ct was already runnable or are at the root we are all done
    if (ct->ct_runnable > 1 || ct->ct_ko.ko_id == user_root_ct)
        return;

    // recurse on parent
    r = container_find(&temp, ct->ct_ko.ko_parent, iflow_none);
    if (r < 0)
        panic("container_join: couldn't get the parent ct of %"PRIu64"\n",
                ct->ct_ko.ko_parent);
    ct = &kobject_dirty(&temp->ct_ko)->ct;
    kobj_id = ct->ct_ko.ko_id;
    goto recurse;
}

// Essentially set kobj_id in ct as not runnable
void
container_leave(struct Container *ct, uint64_t kobj_id)
{
    int r;
    struct container_slot *cs;
    const struct Container *temp;

recurse:
    r = container_slot_find(ct, kobj_id, &cs, page_shared_ro);
    if (r < 0)
        panic("container_leave: %"PRIu64" not in this container\n", kobj_id);

    // this is for the case the same thread is in a container twice
    // this will keep from double counting it and hence nothing propagates
    // up so we are done
    if (!cs->cs_runnable)
        return;

    // mark this slot as non-runnable and adjust the container
    cs->cs_runnable = 0;
    ct->ct_runnable -= 1;

    container_pass_update(ct, 0);
    cs->cs_sched_remain = cs->cs_sched_pass - ct->ct_global_pass;
    ct->ct_global_tickets -= cs->cs_sched_tickets;

    // if ct wasn't runnable or we are at the root we are all done
    if (ct->ct_runnable == 0 || ct->ct_ko.ko_id == user_root_ct)
        return;

    // recurse on parent
    r = container_find(&temp, ct->ct_ko.ko_parent, iflow_none);
    if (r < 0)
        panic("container_join: couldn't get the parent ct of %"PRIu64"\n",
                ct->ct_ko.ko_parent);
    ct = &kobject_dirty(&temp->ct_ko)->ct;
    kobj_id = ct->ct_ko.ko_id;
    goto recurse;
}

int
container_schedule(const struct Container *ct)
{
    int r;
    struct container_slot *cs = 0, *min_pass_cs = 0;
    uint64_t slot, slots;
    const struct kobject *kobj;

recurse:
    // find the min scheduleable obj in this ct
    slots = container_nslots(ct);
    for (slot = 0; slot < slots; slot++) {
        r = container_slot_get(ct, slot, &cs, page_shared_ro);
        if (r < 0 || !cs->cs_runnable || !cs->cs_sched_tickets)
            continue;
        if (!min_pass_cs ||
                cs->cs_sched_pass < min_pass_cs->cs_sched_pass)
            min_pass_cs = cs;
    }
    // if none, then we don't run anything
    if (!min_pass_cs) {
        cur_ct = 0;
        cur_thread = 0;
        return -E_NOT_FOUND;
    }

    // If thread schedule it, otherwise "recurse" on the child container
    r = kobject_get(cs->cs_id, &kobj, kobj_any, iflow_none);
    if (r < 0)
        panic("container_schedule: Couldn't get object for chosen slot\n");
    if (kobj->hdr.ko_type == kobj_thread) {
        cur_ct = ct;
        cur_thread = &kobj->th;
        return 0;
    }
    ct = &kobj->ct;
    goto recurse;
}

void
sched_stop(uint64_t elapsed)
{   
    int r;
    struct container_slot *cs;
    const struct Container *ct;
    uint64_t kobj_id;

    // start at current thread and container
    kobj_id = cur_thread->th_ko.ko_id;
    ct = cur_ct;

recurse:
    // find the slot for this thread/container
    r = container_slot_find(ct, kobj_id, &cs, page_excl_dirty);
    if (r < 0)
        panic("sched_stop: couldn't find slot for a ct/cs pair");
    
    // update it's pass
    uint64_t tickets = cs->cs_sched_tickets ? : 1;
    uint128_t cs_stride = stride1 / tickets;
    cs->cs_sched_pass += cs_stride * elapsed;

    // make kobj_id be the id for this ct to update it's pass
    // and update ct to be kobj_id's parent, and loop
    kobj_id = ct->ct_ko.ko_id;
    // unless we are at the root, in which case we are done
    if (kobj_id == user_root_ct)
        return;
    r = container_find(&ct, kobj_id, iflow_none);
    if (r < 0)
        panic("sched_stop: couldn't find a parent ct updating passes\n");

    goto recurse;
}

