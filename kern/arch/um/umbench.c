#include <stdio.h>
#include <machine/um.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/timer.h>
#include <kern/handle.h>
#include <inc/error.h>

void
um_bench(void)
{
    struct Container *c;
    struct Segment *s[20];
    struct Label *l1, *l2;
    struct kobject *ko;
    const struct kobject *cko;
    void *p;

    assert(0 == label_alloc(&l1, 1));
    assert(0 == label_alloc(&l2, 1));

    assert(0 == label_set(l1, 123, LB_LEVEL_STAR));
    assert(0 == label_set(l2, 123, 3));

    assert(0 == label_set(l1, 456, 0));
    assert(0 == label_set(l2, 789, 3));

    assert(0 == container_alloc(l1, &c));
    kobject_incref_resv(&c->ct_ko, 0);
    c->ct_ko.ko_quota_total = CT_QUOTA_INF;

    for (uint32_t i = 0; i < sizeof(s) / sizeof(*s); i++) {
	assert(0 == segment_alloc(l1, &s[i]));
	assert(0 == container_put(c, &s[i]->sg_ko));
    }

    uint64_t t0, t1;
    uint64_t count = 1000000;

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == label_compare(l1, l2, label_leq_starlo, 0));
    t1 = timer_user_nsec();
    cprintf("Non-cached label comparison: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == label_compare(l1, l2, label_leq_starlo, 1));
    t1 = timer_user_nsec();
    cprintf("Cached label comparison: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(-E_LABEL == label_compare(l2, l1, label_leq_starlo, 0));
    t1 = timer_user_nsec();
    cprintf("Non-cached label error: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(handle_alloc() != 0);
    t1 = timer_user_nsec();
    cprintf("Category/object ID alloc: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++) {
	assert(0 == page_alloc(&p));
	page_free(p);
    }
    t1 = timer_user_nsec();
    cprintf("Page alloc/free: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	kobject_gc_scan();
    t1 = timer_user_nsec();
    cprintf("kobject GC scan: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++) {
	assert(0 == kobject_alloc(kobj_label, 0, &ko));
	kobject_gc_scan();
    }
    t1 = timer_user_nsec();
    cprintf("kobject alloc/GC: %"PRIu64" nsec\n", (t1 - t0) / count);

    assert(0 == kobject_alloc(kobj_label, 0, &ko));
    assert(0 == kobject_set_nbytes(&ko->hdr, 8 * PGSIZE));
    kobject_incref_resv(&ko->hdr, 0);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == kobject_get_page(&ko->hdr, (i % 8), &p, page_shared_ro));
    t1 = timer_user_nsec();
    cprintf("kobject get page: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == kobject_get(ko->hdr.ko_id, &cko, kobj_label, iflow_none));
    t1 = timer_user_nsec();
    cprintf("kobject get: %"PRIu64" nsec\n", (t1 - t0) / count);

    for (uint64_t j = 0; j < sizeof(s) / sizeof(*s); j++) {
	t0 = timer_user_nsec();
	for (uint64_t i = 0; i < count; i++)
	    assert(0 == cobj_get(COBJ(c->ct_ko.ko_id, s[j]->sg_ko.ko_id),
				 kobj_segment, &cko, iflow_none));
	t1 = timer_user_nsec();
	cprintf("cobj_get %02"PRIu64"'th: %"PRIu64" nsec\n",
		j, (t1 - t0) / count);
    }
}
