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
reserve_transfer(struct Reserve *src, struct Reserve *dest, uint64_t amount)
{
    if (src->rs_level < amount)
	return -E_NO_SPACE;

    src->rs_level -= amount;
    dest->rs_level += amount;

    return 0;
}

// Caller must guarantee the creating thread can create an object with this label
int
reserve_split(const struct Label *l, struct Reserve *origrs, struct Reserve **newrsp, uint64_t new_level)
{
    if (origrs->rs_level < new_level)
	return -E_NO_SPACE;

    struct Reserve *newrs;
    int r = reserve_alloc(l, &newrs);
    if (r < 0)
	return r;

    // shouldn't be possible to fail because of check at beginning of function
    assert(!reserve_transfer(origrs, newrs, new_level));

    *newrsp = newrs;
    return 0;
}

