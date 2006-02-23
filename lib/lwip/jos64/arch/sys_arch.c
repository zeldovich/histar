#include <inc/lib.h>
#include <inc/pthread.h>
#include <inc/syscall.h>
#include <inc/queue.h>

#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define NSEM		256
#define NMBOX		64
#define MBOXSLOTS	32

struct sys_sem_entry {
    atomic64_t inuse;
    atomic64_t counter;
};
static struct sys_sem_entry sems[NSEM];

struct sys_mbox_entry {
    atomic_t inuse;
    int head, nextq;
    void *msg[MBOXSLOTS];
    pthread_mutex_t mutex;
    sys_sem_t queued_msg;
    sys_sem_t free_msg;
};
static struct sys_mbox_entry mboxes[NMBOX];

struct sys_thread {
    uint64_t tid;
    struct sys_timeouts tmo;
    LIST_ENTRY(sys_thread) link;
};
static LIST_HEAD(thread_list, sys_thread) threads;

void
sys_init()
{
}

sys_mbox_t
sys_mbox_new()
{
    int i;
    for (i = 0; i < NMBOX; i++) {
	if (atomic_compare_exchange(&mboxes[i].inuse, 0, 1) == 0)
	    break;
    }

    if (i == NMBOX) {
	cprintf("lwip: sys_mbox_new: out of mailboxes\n");
	return SYS_MBOX_NULL;
    }

    mboxes[i].head = -1;
    mboxes[i].nextq = 0;
    mboxes[i].queued_msg = sys_sem_new(0);
    mboxes[i].free_msg = sys_sem_new(MBOXSLOTS);

    if (mboxes[i].queued_msg == SYS_SEM_NULL ||
	mboxes[i].free_msg == SYS_SEM_NULL)
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
    sys_sem_free(mboxes[mbox].queued_msg);
    sys_sem_free(mboxes[mbox].free_msg);
    atomic_set(&mboxes[mbox].inuse, 0);
}

void
sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    sys_arch_sem_wait(mboxes[mbox].free_msg, 0);
    pthread_mutex_lock(&mboxes[mbox].mutex);

    if (mboxes[mbox].nextq == mboxes[mbox].head)
	panic("lwip: sys_mbox_post: no space for message");

    int slot = mboxes[mbox].nextq;
    mboxes[mbox].nextq = (slot + 1) % MBOXSLOTS;
    mboxes[mbox].msg[slot] = msg;

    if (mboxes[mbox].head == -1)
	mboxes[mbox].head = slot;

    pthread_mutex_unlock(&mboxes[mbox].mutex);
    sys_sem_signal(mboxes[mbox].queued_msg);
}

sys_sem_t
sys_sem_new(u8_t count)
{
    int i;
    for (i = 0; i < NSEM; i++) {
	if (atomic_compare_exchange64(&sems[i].inuse, 0, 1) == 0)
	    break;
    }

    if (i == NSEM) {
	cprintf("lwip: sys_sem_new: out of semaphores\n");
	return SYS_SEM_NULL;
    }

    atomic_set(&sems[i].counter, count);
    return i;
}

void
sys_sem_free(sys_sem_t sem)
{
    atomic_set(&sems[sem].inuse, 0);
}

void
sys_sem_signal(sys_sem_t sem)
{
    atomic_inc64(&sems[sem].counter);
    sys_thread_sync_wakeup(&sems[sem].counter.counter);
}

u32_t
sys_arch_sem_wait(sys_sem_t sem, u32_t tm_msec)
{
    u32_t waited = 0;

    while (tm_msec == 0 || waited < tm_msec) {
	uint64_t v = atomic_read(&sems[sem].counter);
	if (v > 0) {
	    if (v == atomic_compare_exchange64(&sems[sem].counter, v, v-1))
		return waited;
	} else {
	    uint64_t a = sys_clock_msec();
	    uint64_t sleep_until = tm_msec ? a + tm_msec - waited : ~0UL;
	    lwip_core_unlock();
	    sys_thread_sync_wait(&sems[sem].counter.counter, 0, sleep_until);
	    lwip_core_lock();
	    uint64_t b = sys_clock_msec();
	    waited += (b - a);
	}
    }

    return SYS_ARCH_TIMEOUT;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t tm_msec)
{
    int waited = sys_arch_sem_wait(mboxes[mbox].queued_msg, tm_msec);
    if (waited == SYS_ARCH_TIMEOUT)
	return waited;

    pthread_mutex_lock(&mboxes[mbox].mutex);

    int slot = mboxes[mbox].head;
    if (slot == -1)
	panic("lwip: sys_arch_mbox_fetch: no message");
    if (msg)
	*msg = mboxes[mbox].msg[slot];

    mboxes[mbox].head = (slot + 1) % MBOXSLOTS;
    if (mboxes[mbox].head == mboxes[mbox].nextq)
	mboxes[mbox].head = -1;

    pthread_mutex_unlock(&mboxes[mbox].mutex);
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
    uint64_t container = start_env->container;
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
    int64_t tid = thread_id();

    struct sys_thread *t;
    LIST_FOREACH(t, &threads, link)
	if (t->tid == tid)
	    goto out;

    t = malloc(sizeof(*t));
    if (t == 0)
	panic("sys_arch_timeouts: cannot malloc");

    t->tid = tid;
    memset(&t->tmo, 0, sizeof(t->tmo));
    LIST_INSERT_HEAD(&threads, t, link);

    // XXX need a callback when threads exit this address space,
    // so that we can GC these thread-specific structs.

out:
    return &t->tmo;
}

// A lock that serializes all LWIP code
static pthread_mutex_t lwip_core_mu;

void
lwip_core_lock(void)
{
    pthread_mutex_lock(&lwip_core_mu);
}

void
lwip_core_unlock(void)
{
    pthread_mutex_unlock(&lwip_core_mu);
}
