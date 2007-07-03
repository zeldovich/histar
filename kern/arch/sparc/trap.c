#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <machine/trap.h>
#include <inc/error.h>

static void
trapframe_print(const struct Trapframe *tf)
{
    cprintf("       globals     outs   locals      ins\n");
    for (uint32_t i = 0; i < 8; i++) {
	cprintf("   %d: %08x %08x %08x %08x\n", 
		i, tf->tf_reg[i], tf->tf_reg[i + 8],
		tf->tf_reg[i + 16], tf->tf_reg[i + 24]);
    }
    cprintf("\n");
    cprintf(" psr: %08x  y: %08x  pc: %08x  npc: %08x\n",
	    tf->tf_psr, tf->tf_y, tf->tf_pc, tf->tf_npc);
}

void __attribute__((__noreturn__, no_instrument_function))
trap_handler(struct Trapframe *tf)
{
    trapframe_print(tf);
    cprintf("looping\n");
    for (;;) {}
}

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

void
thread_arch_idle(void)
{
    panic("thread_arch_idle");
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_INVAL;
}

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    /* XXX */
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}

void
karch_fp_init(struct Fpregs *fpreg)
{
    /* XXX */
}

int
thread_arch_is_masked(const struct Thread *t)
{
    /* XXX */
    return 0;
}
