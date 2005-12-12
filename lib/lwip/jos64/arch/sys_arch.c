#include <inc/lib.h>
#include <inc/atomic.h>
#include <inc/syscall.h>

#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define NSEM		256
#define NMBOX		64
#define MBOXSLOTS	32
#define NTHREADS	32

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
    atomic_t inuse;
    struct sys_timeouts tmo;
    uint64_t tid;
    struct cobj_ref tobj;
    void (*start) (void *);
    void *arg;
};
static struct sys_thread threads[NTHREADS];

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

static void __attribute__((noreturn))
sys_thread_entry(void *arg)
{
    int slot = (uint64_t) arg;
    threads[slot].tid = thread_id();
    memset(&threads[slot].tmo, 0, sizeof(threads[slot].tmo));
    threads[slot].start(threads[slot].arg);
    atomic_set(&threads[slot].inuse, 0);
    sys_obj_unref(threads[slot].tobj);
    thread_halt();
}

sys_thread_t
sys_thread_new(void (* thread)(void *arg), void *arg, int prio)
{
    uint64_t i;
    for (i = 0; i < NTHREADS; i++) {
	if (atomic_compare_exchange(&threads[i].inuse, 0, 1) == 0)
	    break;
    }

    if (i == NTHREADS)
	panic("lwip: sys_thread_new: out of thread structs\n");

    threads[i].start = thread;
    threads[i].arg = arg;

    int container = start_arg;
    int r = thread_create(container, &sys_thread_entry, (void*)i, &threads[i].tobj);
    if (r < 0)
	panic("lwip: sys_thread_new: cannot create: %s\n", e2s(r));

    return i;
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
    int64_t tid = thread_id();

    int i;
    for (i = 0; i < NTHREADS; i++)
	if (threads[i].tid == tid)
	    break;

    // Handle externally-started threads (XXX potentially a problem)
    static struct sys_timeouts global_tmo;
    if (i == NTHREADS)
	return &global_tmo;

    return &threads[i].tmo;
}
