#include <kern/lib.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/reserve.h>
#include <inc/error.h>

enum { debug_reserves = 0 };

uint64_t reserve_profile = 0;

struct Reserve *root_rs = 0;

struct Reserve_list reserve_list;

static void
reserve_unlink(struct Reserve *rs)
{
    if (rs->rs_linked) {
	LIST_REMOVE(rs, rs_link);
	rs->rs_linked = 0;
    }
}

static void
reserve_link(struct Reserve *rs, struct Reserve_list *rs_list)
{
    assert(!rs->rs_linked);
    LIST_INSERT_HEAD(rs_list, rs, rs_link);
    rs->rs_linked = 1;
}

int
reserve_gc(struct Reserve *rs)
{
    if (debug_reserves)
	cprintf("reserve_gc\n");
    reserve_unlink(rs);
    return 0;
}

// Don't call this from outside this module
// users must use reserve_split
int
reserve_alloc(const struct Label *l, struct Reserve **rsp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_reserve, l, 0, &ko);
    if (r < 0)
        return r;

    struct Reserve *rs = &kobject_dirty(&ko->hdr)->rs;
    ko->hdr.ko_flags = KOBJ_FIXED_QUOTA;

    rs->rs_level = 0;
    rs->rs_consumed = 0;
    rs->rs_decayed = 0;

    rs->rs_linked = 0;
    reserve_link(rs, &reserve_list);

    *rsp = rs;
    return 0;
}

// Caller needs to check iflow between reserves wrt to thread label
static int
reserve_do_transfer(struct Reserve *src, struct Reserve *dest, int64_t amount)
{
    if (amount < 0 || src->rs_level < amount)
	return -E_NO_SPACE;

    src->rs_level -= amount;
    dest->rs_level += amount;

    return 0;
}

int
reserve_transfer(struct cobj_ref sourceref, struct cobj_ref sinkref, int64_t amount)
{
    int64_t r;

    const struct kobject *sourceko;
    r = cobj_get(sourceref, kobj_reserve, &sourceko, iflow_rw);
    if (r < 0)
	return r;
    struct Reserve *source = &kobject_dirty(&sourceko->hdr)->rs;

    const struct kobject *sinkko;
    r = cobj_get(sinkref, kobj_reserve, &sinkko, iflow_rw);
    if (r < 0)
	return r;
    struct Reserve *sink = &kobject_dirty(&sinkko->hdr)->rs;

    return reserve_do_transfer(source, sink, amount);
}

// amount here is 1/1024ths to transfer
int
reserve_transfer_proportional(struct cobj_ref sourceref, struct cobj_ref sinkref, int64_t frac)
{
    int64_t r;

    assert(frac <= 1024);

    const struct kobject *sourceko;
    r = cobj_get(sourceref, kobj_reserve, &sourceko, iflow_rw);
    if (r < 0)
	return r;
    struct Reserve *source = &kobject_dirty(&sourceko->hdr)->rs;

    // If no energy we're done
    if (source->rs_level <= 0)
	return 0;

    const struct kobject *sinkko;
    r = cobj_get(sinkref, kobj_reserve, &sinkko, iflow_rw);
    if (r < 0)
	return r;
    struct Reserve *sink = &kobject_dirty(&sinkko->hdr)->rs;

    int64_t amount = (source->rs_level * frac) >> 10;
    source->rs_level -= amount;
    sink->rs_level += amount;

    return 0;
}

// returns success or failure
int64_t
reserve_consume(struct Reserve *rs, int64_t amount, uint64_t force)
{
    if (!force && rs->rs_level < amount)
	return -E_NO_SPACE;

    rs->rs_level -= amount;
    rs->rs_consumed += amount;

    return 0;
}

static void
reserve_prof_dump(struct Reserve *rs, uint64_t ts)
{
    cprintf("Reserve %"PRIu64" %s"
	    " %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64"\n",
	    rs->rs_ko.ko_id, &rs->rs_ko.ko_name[0],
	    rs->rs_level, rs->rs_consumed, rs->rs_decayed, ts);
}

void
reserve_decay_all(uint64_t now)
{
    struct Reserve *rs;
    LIST_FOREACH(rs, &reserve_list, rs_link)
	do {
	    if (rs->rs_level > 0) {
		// TODO - this magic 12 should be calculated dynamically
		// +1 since otherwise decay won't kick in until apps
		// have 2**12 mJ
		int64_t decay = (rs->rs_level >> 12) + 1;
		rs->rs_level -= decay;
		rs->rs_decayed += decay;
	    }
	    if (reserve_profile)
		reserve_prof_dump(rs, now);
	} while (0);
}

void
reserve_prof_toggle()
{
    reserve_profile = !reserve_profile;
}
