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
    atomic_t inuse;
    atomic_t counter;
};
static struct sys_sem_entry sems[NSEM];
static int sem_sleep_msec = 10;

struct sys_mbox_entry {
    atomic_t inuse;
    int head, nextq;
    void *msg[MBOXSLOTS];
    sys_sem_t mutex;
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
    mboxes[i].mutex = sys_sem_new(1);
    mboxes[i].queued_msg = sys_sem_new(0);
    mboxes[i].free_msg = sys_sem_new(MBOXSLOTS);

    if (mboxes[i].mutex == SYS_SEM_NULL ||
	mboxes[i].queued_msg == SYS_SEM_NULL ||
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
    sys_sem_free(mboxes[mbox].mutex);
    sys_sem_free(mboxes[mbox].queued_msg);
    sys_sem_free(mboxes[mbox].free_msg);
    atomic_set(&mboxes[mbox].inuse, 0);
}

void
sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    sys_arch_sem_wait(mboxes[mbox].free_msg, 0);
    sys_arch_sem_wait(mboxes[mbox].mutex, 0);

    if (mboxes[mbox].nextq == mboxes[mbox].head)
	panic("lwip: sys_mbox_post: no space for message");

    int slot = mboxes[mbox].nextq;
    mboxes[mbox].nextq = (slot + 1) % MBOXSLOTS;
    mboxes[mbox].msg[slot] = msg;

    if (mboxes[mbox].head == -1)
	mboxes[mbox].head = slot;

    sys_sem_signal(mboxes[mbox].mutex);
    sys_sem_signal(mboxes[mbox].queued_msg);
}

sys_sem_t
sys_sem_new(u8_t count)
{
    int i;
    for (i = 0; i < NSEM; i++) {
	if (atomic_compare_exchange(&sems[i].inuse, 0, 1) == 0)
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
    atomic_inc(&sems[sem].counter);
}

u32_t
sys_arch_sem_wait(sys_sem_t sem, u32_t tm_msec)
{
    u32_t waited = 0;

    while (tm_msec == 0 || waited < tm_msec) {
	int v = atomic_read(&sems[sem].counter);
	if (v > 0) {
	    if (v == atomic_compare_exchange(&sems[sem].counter, v, v-1))
		return waited;
	} else {
	    sys_thread_sleep(sem_sleep_msec);
	    waited += sem_sleep_msec;
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

    sys_arch_sem_wait(mboxes[mbox].mutex, 0);

    int slot = mboxes[mbox].head;
    if (slot == -1)
	panic("lwip: sys_arch_mbox_fetch: no message");
    if (msg)
	*msg = mboxes[mbox].msg[slot];

    mboxes[mbox].head = (slot + 1) % MBOXSLOTS;
    if (mboxes[mbox].head == mboxes[mbox].nextq)
	mboxes[mbox].head = -1;

    sys_sem_signal(mboxes[mbox].free_msg);
    sys_sem_signal(mboxes[mbox].mutex);
    return waited;
}

sys_thread_t
sys_thread_new(void (* thread)(void *arg), void *arg, int prio)
{
    uint64_t container = start_env->container;
    struct cobj_ref tobj;
    int r = thread_create(container, thread, arg,
			  &tobj, "lwip thread");
    if (r < 0)
	panic("lwip: sys_thread_new: cannot create: %s\n", e2s(r));

    return tobj.object;
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
    int64_t tid = thread_id();

    static pthread_mutex_t tls_mu;
    pthread_mutex_lock(&tls_mu);

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
    pthread_mutex_unlock(&tls_mu);
    return &t->tmo;
}
