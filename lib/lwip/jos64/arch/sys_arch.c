#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

void
sys_init()
{
    //cprintf("lwiparch: sys_init\n");
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
    //cprintf("lwiparch: sys_sem_new\n");
    return 0;
}

void
sys_sem_free(sys_sem_t sem)
{
    //cprintf("lwiparch: sys_sem_free\n");
}

void
sys_sem_signal(sys_sem_t sem)
{
    //cprintf("lwiparch: sys_sem_signal\n");
}

u32_t
sys_arch_sem_wait(sys_sem_t sem, u32_t tm_msec)
{
    //cprintf("lwiparch: sys_arch_sem_wait\n");
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
