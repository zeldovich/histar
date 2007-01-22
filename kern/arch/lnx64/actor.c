#include <machine/types.h>
#include <machine/actor.h>
#include <kern/syscall.h>
#include <kern/label.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <kern/lib.h>
#include <kern/handle.h>

#include <stdio.h>

static uint64_t root_container_id;

void
actor_init(void)
{
    // This is meant to be quite similar to user_bootstrap().


    struct Label *rcl;
    assert(0 == label_alloc(&rcl, 1));

    struct Container *rc;
    assert(0 == container_alloc(rcl, &rc));
    rc->ct_ko.ko_quota_total = CT_QUOTA_INF;
    kobject_incref_resv(&rc->ct_ko, 0);

    root_container_id = rc->ct_ko.ko_id;
}

void
actor_create(struct actor *ar, int tainted)
{
    memset(ar, 0, sizeof(*ar));

    const struct kobject *rc_ko;
    assert(0 == kobject_get(root_container_id, &rc_ko, kobj_container, iflow_none));

    struct Label *tl;
    assert(0 == label_alloc(&tl, 1));

    struct Label *tc;
    assert(0 == label_alloc(&tc, 2));

    if (tainted) {
	uint64_t h = handle_alloc();
	assert(0 == label_set(tl, h, 3));
	assert(0 == label_set(tc, h, 3));

	struct Container *tctr;
	assert(0 == container_alloc(tl, &tctr));
	tctr->ct_ko.ko_quota_total = CT_QUOTA_INF;

	assert(0 == container_put(&kobject_dirty(&rc_ko->hdr)->ct, &tctr->ct_ko));
	ar->scratch_ct = tctr->ct_ko.ko_id;
    } else {
	ar->scratch_ct = root_container_id;
    }

    struct Thread *t;
    assert(0 == thread_alloc(tl, tc, &t));
    assert(0 == container_put(&kobject_dirty(&rc_ko->hdr)->ct, &t->th_ko));
    thread_set_runnable(t);
    thread_set_sched_parents(t, root_container_id, 0);
    ar->thread_id = t->th_ko.ko_id;
}

static void
context_restore(struct actor_context *ac)
{
    if (ac->handle_counter)
	handle_counter = ac->handle_counter;
}

static void
context_save(struct actor_context *ac)
{
    ac->handle_counter = handle_counter;
}

void
action_run(struct actor *ar, struct actor_context *ac, struct action *an, struct action_result *r)
{
    context_restore(ac);

    const struct kobject *th_ko;
    assert(0 == kobject_get(ar->thread_id, &th_ko, kobj_thread, iflow_none));
    cur_thread = &th_ko->th;

    memset(r, 0, sizeof(*r));
    switch (an->type) {
    case actor_action_noop:
	break;

    case actor_action_create_segment:
	r->rval = kern_syscall(SYS_segment_create,
			       ar->scratch_ct,
			       0, 0, 0,
			       0, 0, 0);
	break;

    default:
	printf("action_run: unknown action %d\n", an->type);
    }

    context_save(ac);
}
