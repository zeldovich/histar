#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/part.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <machine/trap.h>
#include <machine/srmmu.h>
#include <machine/pmap.h>
#include <machine/sparc-common.h>
#include <dev/apbucons.h>
#include <dev/amba.h>
#include <dev/irqmp.h>
#include <dev/gptimer.h>

#include <inc/setjmp.h>

char boot_cmdline[256];

static void
mmu_init(void)
{
    for (uint32_t i = 64; i < 128; i++)
	bootpt.pm1_ent[i] = 0;
    tlb_flush_all();
}

static void
bss_init (void)
{
    extern char sbss[], ebss[];
    memset(sbss, 0, ebss - sbss);
}

void __attribute__((noreturn))
init (void)
{
    mmu_init();
    bss_init();

    amba_init();
    apbucons_init();
    amba_print();
    
    irqmp_init();    
    gptimer_init();

    page_init();
    kobject_init();
    sched_init();
    pstate_init();

    user_init();

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();

}
