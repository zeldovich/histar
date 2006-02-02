#include <machine/memlayout.h>
#include <kern/mlt.h>
#include <kern/kobj.h>
#include <inc/error.h>

#define MLT_SLOTS_PER_PAGE	(PGSIZE / sizeof(struct mlt_entry))

int
mlt_alloc(const struct Label *l, struct Mlt **mtp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_mlt, l, &ko);
    if (r < 0)
	return r;

    *mtp = &ko->u.mt;
    return 0;
}

static uint64_t
mlt_nslots(const struct Mlt *mlt)
{
    return mlt->mt_ko.ko_npages * MLT_SLOTS_PER_PAGE;
}

static int
mlt_get_slot(const struct Mlt *mlt, struct mlt_entry **mep,
	     uint64_t slot, page_rw_mode rw)
{
    int npage = slot / MLT_SLOTS_PER_PAGE;

    void *p;
    int r = kobject_get_page(&mlt->mt_ko, npage, &p, rw);
    if (r < 0)
	return r;

    struct mlt_entry *meps = (struct mlt_entry *) p;
    *mep = &meps[slot % MLT_SLOTS_PER_PAGE];
    return 0;
}

static int
mlt_grow(struct Mlt *mlt, struct mlt_entry **mep)
{
    uint64_t npage = mlt->mt_ko.ko_npages;
    int r = kobject_set_npages(&mlt->mt_ko, npage + 1);
    if (r < 0)
	return r;

    void *p;
    r = kobject_get_page(&mlt->mt_ko, npage, &p, page_rw);
    if (r < 0)
	return r;

    *mep = (struct mlt_entry *) p;
    return 0;
}

int
mlt_put(const struct Mlt *mlt, uint8_t *buf)
{
    struct mlt_entry *me = 0;
    const struct Label *l = &cur_thread->th_ko.ko_label;

    int r;
    struct mlt_entry *freeslot = 0;
    uint64_t slot, nslots = mlt_nslots(mlt);

    for (slot = 0; slot < nslots; slot++) {
	r = mlt_get_slot(mlt, &me, slot, page_rw);
	if (r < 0)
	    return r;

	if (!me->me_inuse) {
	    freeslot = me;
	    continue;
	}

	r = label_compare(l, &me->me_l, label_eq);
	if (r == 0)
	    break;
    }

    if (slot == nslots) {
	if (freeslot)
	    me = freeslot;

	if (!me) {
	    r = mlt_grow(&kobject_dirty(&mlt->mt_ko)->u.mt, &me);
	    if (r < 0)
		return r;
	}
    }

    me->me_l = *l;
    memcpy(&me->me_buf[0], buf, MLT_BUF_SIZE);
    me->me_inuse = 1;
    return 0;
}

int
mlt_get(const struct Mlt *mlt, uint8_t *buf)
{
    uint64_t nslots = mlt_nslots(mlt);

    for (uint64_t slot = 0; slot < nslots; slot++) {
	struct mlt_entry *me;
	int r = mlt_get_slot(mlt, &me, slot, page_ro);
	if (r < 0)
	    return r;

	if (!me->me_inuse)
	    continue;

	r = label_compare(&me->me_l, &cur_thread->th_ko.ko_label,
			  label_leq_starhi);
	if (r < 0)
	    continue;

	memcpy(buf, &me->me_buf[0], MLT_BUF_SIZE);
	return 0;
    }

    return -E_NOT_FOUND;
}
