#include <kern/lib.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/limit.h>
#include <kern/reserve.h>
#include <inc/error.h>
#include <kern/timer.h>
#include <kern/reserve.h>

enum { debug_limits = 0 };

struct Limit_list limit_list;
uint64_t limits_last_updated = 0;

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

    lm->lm_level = 0;
    lm->lm_limit = 0;

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

    //struct Reserve *sourcers = &kobject_dirty(&ko->hdr)->rs;

    r = cobj_get(sinkrsref, kobj_reserve, &ko, iflow_rw);
    if (r < 0)
	return r;
    //struct Reserve *sinkrs = &kobject_dirty(&ko->hdr)->rs;

    struct Limit *lm;
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
limit_set_rate(struct Limit *lm, uint64_t rate)
{
    lm->lm_rate = rate;

    return 0;
}

// uses and modifies limits_last_updated global
void
limit_update_all(void)
{
    uint64_t now = timer_user_nsec();

    if (now - limits_last_updated < 1 * 1000 * 1000 * 1000)
	return;

    struct Limit *lm;
    int r;
    struct Limit *last_lm;
    LIST_FOREACH(lm, &limit_list, lm_link)
	do {
	    if (debug_limits)
		cprintf("Working on limit %lu (transferring %lu)\n", lm->lm_ko.ko_id, lm->lm_rate);
	    r = reserve_transfer(lm->lm_source, lm->lm_sink, lm->lm_rate);
	    if (r < 0) {
		if (debug_limits)
		    cprintf("source was out of energy\n");
	    }
	    if (lm)
		last_lm = lm;
	} while (0);
    // Move the last_lm to the head of the list
    // to prevent starvation
    if (last_lm) {
	limit_unlink(last_lm);
	limit_link(last_lm, &limit_list);
    }

    limits_last_updated = now;
}
