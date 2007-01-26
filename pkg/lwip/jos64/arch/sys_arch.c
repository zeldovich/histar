#include <inc/lib.h>
#include <inc/jthread.h>
#include <inc/syscall.h>
#include <inc/queue.h>

#include <stdlib.h>
#include <string.h>

#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define NSEM		256
#define NMBOX		64
#define MBOXSLOTS	32

struct sys_sem_entry {
    int freed;
    int gen;
    union {
	uint64_t v;
	struct {
	    uint32_t counter;
	    uint32_t waiters;
	};
    };
    LIST_ENTRY(sys_sem_entry) link;
};
static struct sys_sem_entry sems[NSEM];
static LIST_HEAD(sem_list, sys_sem_entry) sem_free;

struct sys_mbox_entry {
    int freed;
    int head, nextq;
    void *msg[MBOXSLOTS];
    sys_sem_t queued_msg;
    sys_sem_t free_msg;
    LIST_ENTRY(sys_mbox_entry) link;
};
static struct sys_mbox_entry mboxes[NMBOX];
static LIST_HEAD(mbox_list, sys_mbox_entry) mbox_free;

struct sys_thread {
    uint64_t tid;
    struct sys_timeouts tmo;
    LIST_ENTRY(sys_thread) link;
};
enum { thread_hash_size = 257 };
static LIST_HEAD(thread_list, sys_thread) threads[thread_hash_size];

void
sys_init()
{
    for (int i = 0; i < NSEM; i++) {
	sems[i].freed = 1;
	LIST_INSERT_HEAD(&sem_free, &sems[i], link);
    }

    for (int i = 0; i < NMBOX; i++) {
	mboxes[i].freed = 1;
	LIST_INSERT_HEAD(&mbox_free, &mboxes[i], link);
    }
}

sys_mbox_t
sys_mbox_new()
{
    struct sys_mbox_entry *mbe = LIST_FIRST(&mbox_free);
    if (!mbe) {
	cprintf("lwip: sys_mbox_new: out of mailboxes\n");
	return SYS_MBOX_NULL;
    }
    LIST_REMOVE(mbe, link);
    assert(mbe->freed);
    mbe->freed = 0;

    int i = mbe - &mboxes[0];
    mbe->head = -1;
    mbe->nextq = 0;
    mbe->queued_msg = sys_sem_new(0);
    mbe->free_msg = sys_sem_new(MBOXSLOTS);

    if (mbe->queued_msg == SYS_SEM_NULL ||
	mbe->free_msg == SYS_SEM_NULL)
    {
	sys_mbox_free(i);
	cprintf("lwip: sys_mbox_new: can't get semaphore\n");
	return SYS_MBOX_NULL;
    }
    return i;
}

void
sys_mbox_free(sys_mbox_t mbox)
{
    assert(!mboxes[mbox].freed);
    sys_sem_free(mboxes[mbox].queued_msg);
    sys_sem_free(mboxes[mbox].free_msg);
    LIST_INSERT_HEAD(&mbox_free, &mboxes[mbox], link);
    mboxes[mbox].freed = 1;
}

void
sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    assert(!mboxes[mbox].freed);

    sys_arch_sem_wait(mboxes[mbox].free_msg, 0);
    if (mboxes[mbox].nextq == mboxes[mbox].head)
	panic("lwip: sys_mbox_post: no space for message");

    int slot = mboxes[mbox].nextq;
    mboxes[mbox].nextq = (slot + 1) % MBOXSLOTS;
    mboxes[mbox].msg[slot] = msg;

    if (mboxes[mbox].head == -1)
	mboxes[mbox].head = slot;

    sys_sem_signal(mboxes[mbox].queued_msg);
}

sys_sem_t
sys_sem_new(u8_t count)
{
    struct sys_sem_entry *se = LIST_FIRST(&sem_free);
    if (!se) {
	cprintf("lwip: sys_sem_new: out of semaphores\n");
	return SYS_SEM_NULL;
    }
    LIST_REMOVE(se, link);
    assert(se->freed);
    se->freed = 0;

    se->counter = count;
    se->gen++;
    return se - &sems[0];
}

void
sys_sem_free(sys_sem_t sem)
{
    assert(!sems[sem].freed);
    sems[sem].freed = 1;
    sems[sem].gen++;
    LIST_INSERT_HEAD(&sem_free, &sems[sem], link);
}

void
sys_sem_signal(sys_sem_t sem)
{
    assert(!sems[sem].freed);
    sems[sem].counter++;
    if (sems[sem].waiters) {
	sems[sem].waiters = 0;
	sys_sync_wakeup(&sems[sem].v);
    }
}

u32_t
sys_arch_sem_wait(sys_sem_t sem, u32_t tm_msec)
{
    assert(!sems[sem].freed);
    u32_t waited = 0;
    int gen = sems[sem].gen;

    while (tm_msec == 0 || waited < tm_msec) {
	if (sems[sem].counter > 0) {
	    sems[sem].counter--;
	    return waited;
	} else if (tm_msec == SYS_ARCH_NOWAIT) {
	    return SYS_ARCH_TIMEOUT;
	} else {
	    uint64_t a = sys_clock_msec();
	    uint64_t sleep_until = tm_msec ? a + tm_msec - waited : ~0UL;
	    sems[sem].waiters = 1;
	    uint64_t cur_v = sems[sem].v;
	    lwip_core_unlock();
	    sys_sync_wait(&sems[sem].v, cur_v, sleep_until);
	    lwip_core_lock();
	    if (gen != sems[sem].gen) {
		cprintf("sys_arch_sem_wait: sem freed under waiter!\n");
		return SYS_ARCH_TIMEOUT;
	    }
	    uint64_t b = sys_clock_msec();
	    waited += (b - a);
	}
    }

    return SYS_ARCH_TIMEOUT;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t tm_msec)
{
    assert(!mboxes[mbox].freed);

    u32_t waited = sys_arch_sem_wait(mboxes[mbox].queued_msg, tm_msec);
    if (waited == SYS_ARCH_TIMEOUT)
	return waited;

    int slot = mboxes[mbox].head;
    if (slot == -1)
	panic("lwip: sys_arch_mbox_fetch: no message");
    if (msg)
	*msg = mboxes[mbox].msg[slot];

    mboxes[mbox].head = (slot + 1) % MBOXSLOTS;
    if (mboxes[mbox].head == mboxes[mbox].nextq)
	mboxes[mbox].head = -1;

    sys_sem_signal(mboxes[mbox].free_msg);
    return waited;
}

struct lwip_thread {
    void (*func)(void *arg);
    void *arg;
};

static void
lwip_thread_entry(void *arg)
{
    struct lwip_thread *lt = arg;
    lwip_core_lock();
    lt->func(lt->arg);
    lwip_core_unlock();
    free(lt);
}

sys_thread_t
sys_thread_new(void (* thread)(void *arg), void *arg, int prio)
{
    uint64_t container = start_env->proc_container;
    struct cobj_ref tobj;
    struct lwip_thread *lt = malloc(sizeof(*lt));
    if (lt == 0)
	panic("sys_thread_new: cannot allocate thread struct");

    lt->func = thread;
    lt->arg = arg;

    int r = thread_create(container, &lwip_thread_entry, lt,
			  &tobj, "lwip thread");
    if (r < 0)
	panic("lwip: sys_thread_new: cannot create: %s\n", e2s(r));

    return tobj.object;
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
    uint64_t tid = thread_id();

    struct sys_thread *t;
    LIST_FOREACH(t, &threads[tid % thread_hash_size], link)
	if (t->tid == tid)
	    goto out;

    t = malloc(sizeof(*t));
    if (t == 0)
	panic("sys_arch_timeouts: cannot malloc");

    t->tid = tid;
    memset(&t->tmo, 0, sizeof(t->tmo));
    LIST_INSERT_HEAD(&threads[tid % thread_hash_size], t, link);

    // XXX need a callback when threads exit this address space,
    // so that we can GC these thread-specific structs.

out:
    return &t->tmo;
}

// A lock that serializes all LWIP code, including the above
static jthread_mutex_t lwip_core_mu;

void
lwip_core_lock(void)
{
    jthread_mutex_lock(&lwip_core_mu);
}

void
lwip_core_unlock(void)
{
    jthread_mutex_unlock(&lwip_core_mu);
}
