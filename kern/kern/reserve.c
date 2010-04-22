#include <kern/lib.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <kern/reserve.h>
#include <inc/error.h>

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

// Caller must guarantee the creating thread can create an object with this label
int
reserve_split(const struct Label *l, struct Reserve *origrs, struct Reserve **newrsp, int64_t new_level)
{
    if (new_level < 0 || origrs->rs_level < new_level)
	return -E_NO_SPACE;

    struct Reserve *newrs;
    int r = reserve_alloc(l, &newrs);
    if (r < 0)
	return r;

    // shouldn't be possible to fail because of check at beginning of function
    assert(!reserve_do_transfer(origrs, newrs, new_level));

    *newrsp = newrs;
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

// returns success or failure
int64_t
reserve_consume(struct Reserve *rs, int64_t amount, uint64_t force)
{
    if (!force && rs->rs_level < amount)
	return -E_NO_SPACE;

    rs->rs_level -= amount;

    return 0;
}
