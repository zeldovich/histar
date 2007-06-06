#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    /* XXX */

    for (uint32_t i = 0; i < thread_entry_narg; i++)
	t->th_tfa.tfa_entry_args.te_arg[i] = te->te_arg[i];
}

void
thread_arch_run(const struct Thread *t)
{
    panic("thread_arch_run");
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_INVAL;
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}
