#include <assert.h>
#include <stdio.h>

/* XXX
 * include/asm-jos64/processor.h
 * arch/jos64/kernel/
 */

#define FUNC_PANIC(__name)      \
    void __name(void) { printf("%s not implemented\n", #__name); assert(0); }

FUNC_PANIC(do_settimeofday);

FUNC_PANIC(iounmap);
FUNC_PANIC(__ioremap);
FUNC_PANIC(flush_thread);
FUNC_PANIC(touch_nmi_watchdog);
FUNC_PANIC(dma_alloc_coherent);
FUNC_PANIC(dma_free_coherent);

FUNC_PANIC(show_regs);
FUNC_PANIC(sys_ioperm);
FUNC_PANIC(ioremap_nocache);
FUNC_PANIC(show_interrupts);
FUNC_PANIC(get_wchan);

FUNC_PANIC(kern_addr_valid);
FUNC_PANIC(__switch_to);
FUNC_PANIC(ack_bad_irq);

FUNC_PANIC(machine_power_off);
FUNC_PANIC(machine_halt);
FUNC_PANIC(machine_restart);
FUNC_PANIC(machine_emergency_restart);
FUNC_PANIC(pm_power_off);
FUNC_PANIC(arch_ptrace);
FUNC_PANIC(ptrace_disable);
FUNC_PANIC(profile_pc);

FUNC_PANIC(prepare_to_copy);
FUNC_PANIC(show_stack);
FUNC_PANIC(sync_regs);
FUNC_PANIC(select_idle_routine);
FUNC_PANIC(init_intel_cacheinfo);

FUNC_PANIC(ret_from_fork);
FUNC_PANIC(load_gs_index);
FUNC_PANIC(alternative_instructions);
FUNC_PANIC(identify_cpu);

FUNC_PANIC(free_initmem);
FUNC_PANIC(early_printk);

FUNC_PANIC(thread_saved_pc);
