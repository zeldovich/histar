#include <kern/thread.h>
#include <kern/as.h>
#include <kern/sync.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <inc/error.h>

static struct Thread_list sync_time_waiting;
static struct sync_wait_list sync_addr_waiting;
static int sync_debug = 0;

static int __attribute__((warn_unused_result))
sync_waitslot_init(uint64_t *addr, uint64_t val, struct sync_wait_slot *slot,
		   struct sync_wait_list *swlist, const struct Thread *t)
{
    if (sync_debug)
	cprintf("sync_waitslot_init: addr %p val %"PRIx64"\n", addr, val);

    int r = check_user_access(addr, sizeof(*addr), 0);
    if (r < 0)
	return r;

    if (sync_debug)
	cprintf("sync_waitslot_init: cur val %"PRIx64"\n", *addr);

    if (*addr != val)
	return 0;

    r = as_invert_mapped(cur_thread->th_as, addr,
			 &slot->sw_seg_id, &slot->sw_offset);
    if (r < 0)
	return r;

    if (sync_debug)
	cprintf("sync_waitslot_init: %p -> %"PRIu64", offset %"PRIu64"\n",
		addr, slot->sw_seg_id, slot->sw_offset);

    LIST_INSERT_HEAD(swlist, slot, sw_thread_link);
    slot->sw_t = t;
    return 1;
}

int
sync_wait(uint64_t **addrs, uint64_t *vals, uint64_t num, uint64_t wakeup_msec)
{
    if (sync_debug)
	cprintf("sync_wait: t %p num %"PRIu64" wakeup %"PRIx64" now %"PRIx64"\n",
		cur_thread, num, wakeup_msec, timer_user_msec);

    if (num == 0)
	return 0;

    if (num > cur_thread->th_multi_slots + 1)
	return -E_NO_SPACE;

    if (wakeup_msec <= timer_user_msec)
	return 0;

    int r = thread_load_as(cur_thread);
    if (r < 0)
	return r;

    struct kobject *tko = kobject_ephemeral_dirty(&cur_thread->th_ko);
    struct Thread_ephemeral *te = &tko->ko_th_e;
    struct Thread *t = &kobject_ephemeral_dirty(&cur_thread->th_ko)->th;

    LIST_INIT(&te->te_wait_slots);
    r = sync_waitslot_init(addrs[0], vals[0], &te->te_sync, &te->te_wait_slots, t);
    if (r <= 0)
	return r;

    struct sync_wait_slot *sw;
    uint32_t slots_per_page = PGSIZE / sizeof(*sw);
    for (uint64_t i = 1; i < num; i++) {
	uint64_t pg_num = (i - 1) / slots_per_page + 1;		// skip Fpregs
	uint64_t pg_idx = (i - 1) % slots_per_page;

	r = kobject_get_page(&t->th_ko, pg_num, (void **) &sw, page_excl_dirty);
	if (r < 0)
	    return r;

	r = sync_waitslot_init(addrs[i], vals[i], &sw[pg_idx], &te->te_wait_slots, t);
	if (r <= 0)
	    return r;
    }

    LIST_FOREACH(sw, &te->te_wait_slots, sw_thread_link)
	LIST_INSERT_HEAD(&sync_addr_waiting, sw, sw_addr_link);

    te->te_wakeup_msec = wakeup_msec;
    thread_suspend(cur_thread, &sync_time_waiting);
    t->th_sync_waiting = 1;
    return 0;
}

int
sync_wakeup_addr(uint64_t *addr)
{
    if (sync_debug)
	cprintf("sync_wakeup_addr: addr %p\n", addr);

    int r = thread_load_as(cur_thread);
    if (r < 0)
	return r;

    uint64_t seg_id, offset;
    r = as_invert_mapped(cur_thread->th_as, addr, &seg_id, &offset);
    if (r < 0)
	return r;

    if (sync_debug)
	cprintf("sync_wakeup_addr: %p -> %"PRIu64", offset %"PRIu64"\n",
		addr, seg_id, offset);

    struct sync_wait_slot *sw = LIST_FIRST(&sync_addr_waiting);
    struct sync_wait_slot *prev = 0;
    while (sw != 0) {
	if (sw->sw_seg_id == seg_id && sw->sw_offset == offset) {
	    thread_set_runnable(sw->sw_t);
	    sw = prev ? LIST_NEXT(prev, sw_addr_link) : LIST_FIRST(&sync_addr_waiting);
	} else {
	    prev = sw;
	    sw = LIST_NEXT(sw, sw_addr_link);
	}
    }

    return 0;
}

void
sync_wakeup_timer(void)
{
    if (sync_debug)
	cprintf("sync_wakeup_timer\n");

    struct Thread *t = LIST_FIRST(&sync_time_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);
	struct Thread_ephemeral *te = &kobject_ephemeral_dirty(&t->th_ko)->ko_th_e;

	if (te->te_wakeup_msec <= timer_user_msec) {
	    if (sync_debug)
		cprintf("sync_wakeup_timer: waking up %"PRIx64" now %"PRIx64"\n",
			te->te_wakeup_msec, timer_user_msec);

	    thread_set_runnable(t);
	}

	t = next;
    }
}

void
sync_remove_thread(const struct Thread *t)
{
    if (sync_debug)
	cprintf("sync_remove_thread: %p\n", t);

    struct kobject *tko = kobject_ephemeral_dirty(&t->th_ko);
    assert(tko->th.th_sync_waiting);
    tko->th.th_sync_waiting = 0;

    struct Thread_ephemeral *te = &tko->ko_th_e;
    for (;;) {
	struct sync_wait_slot *sw = LIST_FIRST(&te->te_wait_slots);
	if (!sw)
	    break;

	LIST_REMOVE(sw, sw_thread_link);
	LIST_REMOVE(sw, sw_addr_link);
    }
}
