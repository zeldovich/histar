#include <machine/thread.h>
#include <machine/as.h>
#include <kern/sync.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <inc/error.h>

static struct Thread_list sync_waiting;
static int sync_debug = 0;

#define RET_ERR(__r) \
    if (__r < 0)     \
	return r;

struct waitslots_iter 
{
    const struct Thread *t;
    uint8_t *slots;
    uint32_t pg_off;
    uint64_t slot_page;

    uint64_t *seg_id;
    uint64_t *offset;
};

static void
sync_waitslots_iter(struct waitslots_iter *it, const struct Thread *t) 
{
    memset(it, 0, sizeof(*it));
    it->t = t;
}

static int 
sync_waitslots_next(struct waitslots_iter *it)
{
    int r = 0;
    if (!it->slots) {
	it->pg_off = sizeof(struct Fpregs);
	it->slot_page = 0;
	r = kobject_get_page(&it->t->th_ko, 0, (void **)&it->slots, 
			     page_excl_dirty);
	RET_ERR(r);
    } else {
	it->pg_off += sizeof(uint128_t);
	if ((it->pg_off + sizeof(uint128_t)) > PGSIZE) {
	    it->slot_page++;
	    r = kobject_get_page(&it->t->th_ko, it->slot_page, 
				 (void **)&it->slots, page_excl_dirty);
	    RET_ERR(r);
	    it->pg_off = 0;
	}
    }
    
    it->seg_id = (uint64_t *)&it->slots[it->pg_off];
    it->offset = (uint64_t *)&it->slots[it->pg_off + sizeof(uint64_t)];

    return 0;
}

int
sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_msec)
{
    if (sync_debug)
	cprintf("sync_wait: addr %p val %lx wakeup %lx now %lx\n",
		addr, val, wakeup_msec, timer_user_msec);

    if (*addr != val)
	return 0;
    if (wakeup_msec <= timer_user_msec)
	return 0;

    struct Thread *t = &kobject_ephemeral_dirty(&cur_thread->th_ko)->th;
    t->th_wakeup_msec = wakeup_msec;
    t->th_multi_slots_used = 0;

    int r = as_invert_mapped(t->th_as, addr,
			     &t->th_wakeup_seg_id,
			     &t->th_wakeup_offset);
    if (r < 0)
	return r;

    thread_suspend(cur_thread, &sync_waiting);
    return 0;
}

int
sync_wait_multi(uint64_t **addrs, uint64_t *vals, 
		uint64_t num, uint64_t wakeup_msec)
{
    int r;
    if (sync_debug)
	cprintf("sync_wait_multi: num %ld wakeup %lx now %lx\n",
		 num, wakeup_msec, timer_user_msec);

    if (num == 0)
	return 0;

    if (num > cur_thread->th_multi_slots)
	return -E_NO_SPACE;

    struct Thread *t = &kobject_ephemeral_dirty(&cur_thread->th_ko)->th;
    
    struct waitslots_iter it;
    sync_waitslots_iter(&it, t);
    for (uint64_t i = 0; i < num; i++) {
	if (sync_debug)
	    cprintf("sync_wait_multi: addr %p val %lx\n", addrs[i], vals[i]);

	if (*addrs[i] != vals[i])
	    return 0;
	
	r = sync_waitslots_next(&it);
	RET_ERR(r);
	r = as_invert_mapped(t->th_as, addrs[i],
			     it.seg_id,
			     it.offset);
	RET_ERR(r);
    }
    
    if (wakeup_msec <= timer_user_msec)
	return 0;
    
    t->th_wakeup_msec = wakeup_msec;
    t->th_multi_slots_used = num;
    
    thread_suspend(cur_thread, &sync_waiting);
    
    return 0;
}

int
sync_wakeup_addr(uint64_t *addr)
{
    if (sync_debug)
	cprintf("sync_wakeup_addr: addr %p\n", addr);

    uint64_t seg_id, offset;
    int r = as_invert_mapped(cur_thread->th_as, addr, &seg_id, &offset);
    if (r < 0)
	return r;

    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_multi_slots_used) {
	    struct waitslots_iter it;
	    sync_waitslots_iter(&it, t);
	    
	    for (uint64_t i = 0; i < t->th_multi_slots_used; i++) {
		r = sync_waitslots_next(&it);
		RET_ERR(r);
		if (*it.seg_id == seg_id && *it.offset == offset) {
		    t->th_multi_slots_used = 0;
		    thread_set_runnable(t);
		    break;
		}
	    }
	}
	else if (t->th_wakeup_seg_id == seg_id && t->th_wakeup_offset == offset)
	    thread_set_runnable(t);

	t = next;
    }

    return 0;
}

void
sync_wakeup_timer(void)
{
    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_msec <= timer_user_msec) {
	    if (sync_debug)
		cprintf("sync_wakeup_timer: waking up %lx now %lx\n",
			t->th_wakeup_msec, timer_user_msec);

	    t->th_multi_slots_used = 0;
	    thread_set_runnable(t);
	}

	t = next;
    }
}
