#include <inc/atomic.h>
#include <inc/syscall.h>

#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define NSEM	32
struct sys_sem_slot {
    atomic_t inuse;
    atomic_t counter;
};
static struct sys_sem_slot sems[NSEM];

static int sleep_msec = 10;

void
sys_init()
{
}

sys_mbox_t
sys_mbox_new()
{
    cprintf("lwiparch: sys_mbox_new\n");
    return 0;
}

void
sys_mbox_free(sys_mbox_t mbox)
{
    cprintf("lwiparch: sys_mbox_free\n");
}

void
sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    cprintf("lwiparch: sys_mbox_post\n");
}

sys_sem_t
sys_sem_new(u8_t count)
{
    int i;
    for (i = 0; i < NSEM; i++) {
	if (atomic_compare_exchange(&sems[i].inuse, 0, 1) == 0) {
	    atomic_set(&sems[i].counter, count);
	    break;
	}
    }

    if (i == NSEM) {
	cprintf("lwip: sys_sem_new: out of semaphores\n");
	return SYS_SEM_NULL;
    }

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
	    sys_thread_sleep(sleep_msec);
	    waited += sleep_msec;
	}
    }

    return SYS_ARCH_TIMEOUT;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t tm_msec)
{
    cprintf("lwiparch: sys_arch_mbox_fetch\n");
    return SYS_ARCH_TIMEOUT;
}

sys_thread_t
sys_thread_new(void (* thread)(void *arg), void *arg, int prio)
{
    cprintf("lwiparch: sys_thread_new\n");
    return 0;
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
    static struct sys_timeouts tmo;
    return &tmo;
}
