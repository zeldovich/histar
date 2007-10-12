#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>

void
thread_arch_run(const struct Thread *t)
{
    panic("%s", __func__);
}

void
thread_arch_idle(void)
{
    panic("%s", __func__);
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_NO_MEM;
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    cprintf("%s", __func__);
}

int
thread_arch_is_masked(const struct Thread *t)
{
    cprintf("%s", __func__);
    return 0;
}
