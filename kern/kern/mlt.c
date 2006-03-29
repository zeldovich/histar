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

    *mtp = &ko->mt;
    return 0;
}

static uint64_t
mlt_nslots(const struct Mlt *mlt)
{
    return kobject_npages(&mlt->mt_ko) * MLT_SLOTS_PER_PAGE;
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

int
mlt_gc(struct Mlt *mlt)
{
    uint64_t nslots = mlt_nslots(mlt);
    for (uint64_t i = 0; i < nslots; i++) {
	struct mlt_entry *me;
	int r = mlt_get_slot(mlt, &me, i, page_rw);
	if (r < 0)
	    return r;

	if (!me->me_inuse)
	    continue;

	const struct kobject *ko;
	r = kobject_get(me->me_lb_id, &ko, kobj_label, iflow_none);
	if (r < 0)
	    return r;

	kobject_decref(&ko->hdr);
	me->me_inuse = 0;
    }

    return 0;
}

static int
mlt_grow(struct Mlt *mlt, struct mlt_entry **mep)
{
    uint64_t npage = kobject_npages(&mlt->mt_ko);
    int r = kobject_set_nbytes(&mlt->mt_ko, (npage + 1) * PGSIZE);
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
mlt_put(const struct Mlt *mlt, const struct Label *l, uint8_t *buf)
{
    struct mlt_entry *me = 0;

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

	const struct kobject *me_lb_ko;
	r = kobject_get(me->me_lb_id, &me_lb_ko, kobj_label, iflow_none);
	if (r < 0)
	    return r;
	const struct Label *me_lb = &me_lb_ko->lb;

	r = label_compare(l, me_lb, label_eq);
	if (r == 0)
	    break;
    }

    if (slot == nslots) {
	if (freeslot)
	    me = freeslot;

	if (!me) {
	    r = mlt_grow(&kobject_dirty(&mlt->mt_ko)->mt, &me);
	    if (r < 0)
		return r;
	}
    }

    if (!me->me_inuse) {
	me->me_lb_id = l->lb_ko.ko_id;
	me->me_inuse = 1;
	kobject_incref(&l->lb_ko);
    }

    memcpy(&me->me_buf[0], buf, MLT_BUF_SIZE);
    return 0;
}

int
mlt_get(const struct Mlt *mlt, uint64_t idx, const struct Label **l, uint8_t *buf)
{
    uint64_t nslots = mlt_nslots(mlt);

    const struct Label *cur_th_label;
    int r = kobject_get_label(&cur_thread->th_ko, kolabel_contaminate, &cur_th_label);
    if (r < 0)
	return r;

    for (uint64_t slot = 0; slot < nslots; slot++) {
	struct mlt_entry *me;
	r = mlt_get_slot(mlt, &me, slot, page_ro);
	if (r < 0)
	    return r;

	if (!me->me_inuse)
	    continue;

	const struct kobject *me_lb_ko;
	r = kobject_get(me->me_lb_id, &me_lb_ko, kobj_label, iflow_none);
	if (r < 0)
	    return r;
	const struct Label *me_lb = &me_lb_ko->lb;

	r = label_compare(me_lb, cur_th_label, label_leq_starhi);
	if (r < 0)
	    continue;

	if (idx > 0) {
	    idx--;
	    continue;
	}

	if (buf)
	    memcpy(buf, &me->me_buf[0], MLT_BUF_SIZE);
	if (l)
	    *l = me_lb;

	return 0;
    }

    return -E_NOT_FOUND;
}
