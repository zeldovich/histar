#include <stdio.h>
#include <machine/um.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/timer.h>
#include <kern/id.h>
#include <inc/error.h>

void
um_bench(void)
{
    struct Container *c;
    struct Segment *sg;
    struct Segment *s[20];
    struct Label *l1, *l2, *l3, *l4, *owner, *clear;
    uint64_t cs[16], ci[16], csx;
    struct kobject *ko;
    const struct kobject *cko;
    void *p;

    assert(0 == label_alloc(&l1, label_track));
    assert(0 == label_alloc(&l2, label_track));

    csx = id_alloc() | LB_SECRECY_FLAG;
    assert(0 == label_add(l2, csx));

    assert(0 == label_alloc(&owner, label_priv));
    assert(0 == label_alloc(&clear, label_priv));

    for (int i = 0; i < 16; i++) {
	cs[i] = id_alloc() | LB_SECRECY_FLAG;
	ci[i] = id_alloc() | LB_SECRECY_FLAG;

	assert(0 == label_add(l1, cs[i]));
	assert(0 == label_add(l1, ci[i]));

	assert(0 == label_add(l2, cs[i]));
	assert(0 == label_add(l2, ci[i]));

	assert(0 == label_add(owner, cs[i]));
	assert(0 == label_add(owner, ci[i]));

	assert(0 == label_add(clear, cs[i]));
	assert(0 == label_add(clear, ci[i]));
    }

    assert(0 == label_alloc(&l3, label_track));
    assert(0 == label_alloc(&l4, label_track));

    assert(0 == label_add(l3, cs[14]));
    assert(0 == label_add(l3, ci[14]));

    assert(0 == label_add(l4, cs[15]));
    assert(0 == label_add(l4, ci[15]));

    assert(0 == container_alloc(l1, &c));
    kobject_incref_resv(&c->ct_ko, 0);
    c->ct_ko.ko_quota_total = CT_QUOTA_INF;

    for (uint32_t i = 0; i < sizeof(s) / sizeof(*s); i++) {
	assert(0 == segment_alloc(l1, &s[i]));
	assert(0 == container_put(c, &s[i]->sg_ko, 0));
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
	assert(0 == label_can_flow(l1, l2, 0, 0));
    TEST_END("large label comparison");

    TEST_START
	assert(-E_LABEL == label_can_flow(l2, l1, 0, 0));
    TEST_END("large label error");

    TEST_START
	assert(0 == label_can_flow(l3, l4, owner, clear));
    TEST_END("small label comparison with large priv");

    TEST_START
	assert(id_alloc() != 0);
    TEST_END("id alloc");

    TEST_START {
	assert(0 == page_alloc(&p));
	page_free(p);
    }
    TEST_END("page alloc/free");

    TEST_START
	kobject_gc_scan();
    TEST_END("kobject GC scan");

    TEST_START {
	assert(0 == kobject_alloc(kobj_label, 0, 0, 0, &ko));
	kobject_gc_scan();
    }
    TEST_END("kobject alloc/GC");

    assert(0 == kobject_alloc(kobj_label, 0, 0, 0, &ko));
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

    TEST_START {
	assert(0 == segment_alloc(l1, &sg));
	kobject_gc_scan();
    }
    TEST_END("segment alloc/GC");

    assert(0 == segment_alloc(l1, &sg));
    sg->sg_ko.ko_flags |= KOBJ_FIXED_QUOTA;

    TEST_START {
	assert(0 == container_put(c, &sg->sg_ko, 0));
    }
    TEST_END("container addref");

    TEST_START {
	assert(0 == container_unref(c, &sg->sg_ko, 0));
    }
    TEST_END("container unref");

    TEST_START {
	assert(0 == container_put(c, &sg->sg_ko, 0));
	assert(0 == container_unref(c, &sg->sg_ko, 0));
    }
    TEST_END("container addref/unref");
}
