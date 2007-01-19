#include <inc/queue.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <pthread.h>
#include <sys/time.h>

#include <asm/unistd.h>
#include <linux/unistd.h>
#include <syscall.h> 

#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define NSEM		256
#define NMBOX		128
#define MBOXSLOTS	32

// XXX
int clock_gettime(int clk_id, struct timespec *tp);
#define CLOCK_REALTIME                  0

struct sys_sem_entry {
    pthread_cond_t cond;
    uint32_t counter;
    uint32_t waiters;
    LIST_ENTRY(sys_sem_entry) link;
};
static struct sys_sem_entry sems[NSEM];
static LIST_HEAD(sem_list, sys_sem_entry) sem_free;

struct sys_mbox_entry {
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

// A lock that serializes all LWIP code, including the above
static pthread_mutex_t lwip_core_mu;

static int 
cond_signal(pthread_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

static void
cond_timedwait(pthread_cond_t *restrict cond,
	       pthread_mutex_t *restrict mutex,
	       uint64_t msec)
{
    uint64_t sec_max = 0xffffffff;
    if (msec / 1000 > sec_max)
	msec = sec_max * 1000;

    struct timespec ts;
    ts.tv_sec += msec / 1000;
    ts.tv_nsec += (msec % 1000) * 1000000;

    if ((pthread_cond_timedwait(cond, mutex, &ts) < 0) && errno != ETIMEDOUT)
	lwip_panic("cond_timedwait: pthread_cond_wait: %s\n", strerror(errno));
}

static uint64_t
lwip_clock_msec(void)
{
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    if (r < 0)
	lwip_panic("lwip_msec: gettimeofday %s\n", strerror(errno));
    
    uint64_t ret = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    return ret;
}

void
sys_init()
{
    for (int i = 0; i < NSEM; i++) {
	pthread_cond_init(&sems[i].cond, 0);
	LIST_INSERT_HEAD(&sem_free, &sems[i], link);
    }
    for (int i = 0; i < NMBOX; i++)
	LIST_INSERT_HEAD(&mbox_free, &mboxes[i], link);
}

sys_mbox_t
sys_mbox_new()
{
    struct sys_mbox_entry *mbe = LIST_FIRST(&mbox_free);
    if (!mbe) {
	printf("lwip: sys_mbox_new: out of mailboxes\n");
	return SYS_MBOX_NULL;
    }
    LIST_REMOVE(mbe, link);

    int i = mbe - &mboxes[0];
    mbe->head = -1;
    mbe->nextq = 0;
    mbe->queued_msg = sys_sem_new(0);
    mbe->free_msg = sys_sem_new(MBOXSLOTS);

    if (mbe->queued_msg == SYS_SEM_NULL ||
	mbe->free_msg == SYS_SEM_NULL)
    {
	sys_mbox_free(i);
	printf("lwip: sys_mbox_new: can't get semaphore\n");
	return SYS_MBOX_NULL;
    }

    return i;
}

void
sys_mbox_free(sys_mbox_t mbox)
{
    sys_sem_free(mboxes[mbox].queued_msg);
    sys_sem_free(mboxes[mbox].free_msg);
    LIST_INSERT_HEAD(&mbox_free, &mboxes[mbox], link);
}

void
sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    sys_arch_sem_wait(mboxes[mbox].free_msg, 0);
    if (mboxes[mbox].nextq == mboxes[mbox].head)
	lwip_panic("lwip: sys_mbox_post: no space for message");

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
	printf("lwip: sys_sem_new: out of semaphores\n");
	return SYS_SEM_NULL;
    }
    LIST_REMOVE(se, link);

    se->counter = count;
    return se - &sems[0];
}

void
sys_sem_free(sys_sem_t sem)
{
    LIST_INSERT_HEAD(&sem_free, &sems[sem], link);
}

void
sys_sem_signal(sys_sem_t sem)
{
    sems[sem].counter++;
    if (sems[sem].waiters) {
	sems[sem].waiters = 0;
	cond_signal(&sems[sem].cond);
    }
}

u32_t
sys_arch_sem_wait(sys_sem_t sem, u32_t tm_msec)
{
    u32_t waited = 0;

    while (tm_msec == 0 || waited < tm_msec) {
	if (sems[sem].counter > 0) {
	    sems[sem].counter--;
	    return waited;
	} else if (tm_msec == SYS_ARCH_NOWAIT) {
	    waited = SYS_ARCH_TIMEOUT;
	} else {
	    uint64_t a = lwip_clock_msec();
	    uint64_t sleep_until = tm_msec ? a + tm_msec - waited : ~0UL;
	    sems[sem].waiters = 1;

	    cond_timedwait(&sems[sem].cond, &lwip_core_mu, sleep_until);

	    uint64_t b = lwip_clock_msec();
	    waited += (b - a);
	}
    }
    return SYS_ARCH_TIMEOUT;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t tm_msec)
{
    u32_t waited = sys_arch_sem_wait(mboxes[mbox].queued_msg, tm_msec);
    if (waited == SYS_ARCH_TIMEOUT)
	return waited;

    int slot = mboxes[mbox].head;
    if (slot == -1)
	lwip_panic("lwip: sys_arch_mbox_fetch: no message");
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

static void *
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
    struct lwip_thread *lt = malloc(sizeof(*lt));
    if (lt == 0)
	lwip_panic("sys_thread_new: cannot allocate thread struct");

    lt->func = thread;
    lt->arg = arg;

    pthread_t t;
    int r = pthread_create(&t, 0, &lwip_thread_entry, lt);

    if (r < 0)
	lwip_panic("lwip: sys_thread_new: cannot create: %s\n", strerror(errno));

    return t;
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
    uint64_t tid = pthread_self();

    struct sys_thread *t;
    LIST_FOREACH(t, &threads[tid % thread_hash_size], link)
	if (t->tid == tid)
	    goto out;

    t = malloc(sizeof(*t));
    if (t == 0)
	lwip_panic("sys_arch_timeouts: cannot malloc");

    t->tid = tid;
    memset(&t->tmo, 0, sizeof(t->tmo));
    LIST_INSERT_HEAD(&threads[tid % thread_hash_size], t, link);

    // XXX need a callback when threads exit this address space,
    // so that we can GC these thread-specific structs.

out:
    return &t->tmo;
}

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

void
lwip_core_init(void)
{
    pthread_mutex_init(&lwip_core_mu, 0);
}
