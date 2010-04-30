#include <kern/lib.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/limit.h>
#include <kern/reserve.h>
#include <inc/error.h>
#include <kern/timer.h>
#include <kern/reserve.h>
#include <kern/energy.h>

enum { debug_limits = 0 };

uint64_t limit_profile = 0;

struct Limit_list limit_list;
uint64_t limits_last_updated = 0;
const uint64_t limit_period = 100 * 1000 * 1000;

static void
limit_unlink(struct Limit *lm)
{
    if (lm->lm_linked) {
	LIST_REMOVE(lm, lm_link);
	lm->lm_linked = 0;
    }
}

static void
limit_link(struct Limit *lm, struct Limit_list *lm_list)
{
    assert(!lm->lm_linked);
    LIST_INSERT_HEAD(lm_list, lm, lm_link);
    lm->lm_linked = 1;
}

int
limit_gc(struct Limit *lm)
{
    if (debug_limits)
	cprintf("limit_gc\n");
    limit_unlink(lm);
    return 0;
}

static int
limit_alloc(const struct Label *l, struct Limit **lmp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_limit, l, 0, &ko);
    if (r < 0)
        return r;

    struct Limit *lm = &kobject_dirty(&ko->hdr)->lm;
    ko->hdr.ko_flags = KOBJ_FIXED_QUOTA;

    lm->lm_type = 0;
    lm->lm_rate = 0;

    lm->lm_source = COBJ(0, 0);
    lm->lm_sink = COBJ(0, 0);

    lm->lm_linked = 0;
    limit_link(lm, &limit_list);

    *lmp = lm;

    return 0;
}

int
limit_create(const struct Label *l, struct cobj_ref sourcersref,
	     struct cobj_ref sinkrsref, struct Limit **lmp)
{
    const struct kobject *ko;

    int64_t r = cobj_get(sourcersref, kobj_reserve, &ko, iflow_rw);
    if (r < 0)
	return r;

    r = cobj_get(sinkrsref, kobj_reserve, &ko, iflow_rw);
    if (r < 0)
	return r;

    struct Limit *lm = 0;
    r = limit_alloc(l, &lm);
    if (r < 0)
	return r;

    // default to constant type edge

    lm->lm_source = sourcersref;
    lm->lm_sink = sinkrsref;

    *lmp = lm;

    return 0;
}

int
limit_set_rate(struct Limit *lm, uint64_t type, uint64_t rate)
{
    assert(type == LIMIT_TYPE_CONST || type == LIMIT_TYPE_PROP);

    lm->lm_type = type;
    lm->lm_rate = rate;

    return 0;
}

static void
limit_prof_dump(struct Limit *lm, uint64_t ts)
{
    cprintf("Limit %"PRIu64" %s %"PRIu64".%"PRIu64" %"PRIu64".%"PRIu64
	    " %"PRIu64" %"PRIu64" %"PRIu64"\n",
	    lm->lm_ko.ko_id, &lm->lm_ko.ko_name[0],
	    lm->lm_source.container, lm->lm_source.object,
	    lm->lm_sink.container, lm->lm_sink.object,
	    lm->lm_rate, lm->lm_type, ts);
}

// uses and modifies limits_last_updated global
void
limit_update_all(void)
{
    uint64_t now = timer_user_nsec();
    uint64_t elapsed = now - limits_last_updated;
    if (!limits_last_updated) {
	limits_last_updated = now;
	return;
    }
	
    if (elapsed < limit_period)
	return;

    // subtract baseline cost for running the system for 1 s
    assert(root_rs);
    reserve_consume(root_rs, energy_baseline_mJ(elapsed), 1);

    // first do decay and then do additions
    reserve_decay_all(elapsed, now);

    struct Limit *lm;
    int r;
    struct Limit *last_lm = 0;
    LIST_FOREACH(lm, &limit_list, lm_link)
	do {
	    if (lm->lm_type == LIMIT_TYPE_CONST) {
		r = reserve_transfer(lm->lm_source, lm->lm_sink, lm->lm_rate, elapsed);
		if (r < 0) {
		    if (debug_limits)
			cprintf("source was out of energy\n");
		}
	    } else if (lm->lm_type == LIMIT_TYPE_PROP) {
		reserve_transfer_proportional(lm->lm_source, lm->lm_sink, lm->lm_rate, elapsed);
	    } else {
		assert(0);
	    }
	    if (lm)
		last_lm = lm;
	    if (limit_profile)
		limit_prof_dump(lm, now);
	} while (0);
    // Move the last_lm to the head of the list
    // to prevent starvation
    if (last_lm) {
	limit_unlink(last_lm);
	limit_link(last_lm, &limit_list);
    }

    limits_last_updated = now;
}

void
limit_prof_toggle()
{
    limit_profile = !limit_profile;
}
