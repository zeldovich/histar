#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <machine/trap.h>
#include <machine/mmu.h>
#include <inc/error.h>

static const struct Thread *trap_thread;

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
    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_pc = (uintptr_t) te->te_entry;
    t->th_tf.tf_npc = (uintptr_t) (te->te_entry + 4);
    t->th_tf.tf_reg[TF_SP] = (uintptr_t) te->te_stack;
    t->th_tf.tf_reg[TF_I0] = te->te_arg[0];
    t->th_tf.tf_reg[TF_I1] = te->te_arg[1];
    t->th_tf.tf_reg[TF_I2] = te->te_arg[2];
    t->th_tf.tf_reg[TF_I3] = te->te_arg[3];
    t->th_tf.tf_reg[TF_I4] = te->te_arg[4];
    t->th_tf.tf_reg[TF_I5] = te->te_arg[5];
    
    static_assert(thread_entry_narg == 6);
}

static void
trap_thread_set(const struct Thread *t)
{
    if (trap_thread) {
	kobject_unpin_hdr(&trap_thread->th_ko);
	trap_thread = 0;
    }

    if (t) {
	kobject_pin_hdr(&t->th_ko);
	trap_thread = t;
    }
}

void
thread_arch_run(const struct Thread *t)
{
    trap_thread_set(t);
    trapframe_pop(&t->th_tf);
    //panic("thread_arch_run");
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
    cprintf("XXX karch_jmpbuf_init\n");
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
    cprintf("karch_fp_init: XXX unimpl\n");
}

int
thread_arch_is_masked(const struct Thread *t)
{
    cprintf("thread_arch_is_masked: XXX unimpl\n");
    return 0;
}
