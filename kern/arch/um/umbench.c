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

#define TEST_START						\
    t0 = timer_user_nsec();					\
    for (uint64_t i = 0; i < count; i++)

#define TEST_END(...)						\
    t1 = timer_user_nsec();					\
    cprintf("%10"PRIu64" nsec -- ", (t1 - t0) / count);		\
    cprintf(__VA_ARGS__);					\
    cprintf("\n");

    TEST_START
	assert(0 == label_compare(l1, l2, label_leq_starlo, 0));
    TEST_END("non-cached label comparison");

    TEST_START
	assert(0 == label_compare(l1, l2, label_leq_starlo, 1));
    TEST_END("cached label comparison");

    TEST_START
	assert(-E_LABEL == label_compare(l2, l1, label_leq_starlo, 0));
    TEST_END("non-cached label error");

    TEST_START
	assert(handle_alloc() != 0);
    TEST_END("handle alloc");

    TEST_START {
	assert(0 == page_alloc(&p));
	page_free(p);
    }
    TEST_END("page alloc/free");

    TEST_START
	kobject_gc_scan();
    TEST_END("kobject GC scan");

    TEST_START {
	assert(0 == kobject_alloc(kobj_label, 0, &ko));
	kobject_gc_scan();
    }
    TEST_END("kobject alloc/GC");

    assert(0 == kobject_alloc(kobj_label, 0, &ko));
    assert(0 == kobject_set_nbytes(&ko->hdr, 8 * PGSIZE));
    kobject_incref_resv(&ko->hdr, 0);

    TEST_START
	assert(0 == kobject_get_page(&ko->hdr, (i % 8), &p, page_shared_ro));
    TEST_END("kobject get page");

    TEST_START
	assert(0 == kobject_get(ko->hdr.ko_id, &cko, kobj_label, iflow_none));
    TEST_END("kobject get");

    for (uint64_t j = 0; j < sizeof(s) / sizeof(*s); j++) {
	TEST_START
	    assert(0 == cobj_get(COBJ(c->ct_ko.ko_id, s[j]->sg_ko.ko_id),
				 kobj_segment, &cko, iflow_none));
	TEST_END("cobj_get %02"PRIu64"'th", j);
    }
}
