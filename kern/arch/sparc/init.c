#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/part.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
#include <kern/prof.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/sparc-common.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <dev/apbucons.h>
#include <dev/amba.h>
#include <dev/irqmp.h>
#include <dev/gptimer.h>
#include <dev/greth.h>

char boot_cmdline[256];

static void
bss_init (void)
{
    extern char sbss[], ebss[];
    memset(sbss, 0, ebss - sbss);
}

void __attribute__((noreturn))
init (void)
{
    int r;

    write_tsr(TSR_T);
    bss_init();
    mmu_init();

    amba_init();
    irqmp_init();
    apbucons_init();

    page_init();
    tag_init();
    page_init2();

    gptimer_init();
    //amba_print();

    r = greth_init();
    if (r < 0)
	cprintf("init: greth_init error: %s\n", e2s(r));
    
    kobject_init();
    sched_init();
    pstate_init();
    prof_init();

    user_init();
    tag_init_late();

    cprintf("Kernel init done, disabling trusted mode.. ");
    write_tsr(0);
    cprintf("done.\n");

    uint32_t test_in  = 0x521835ab;
    uint32_t test_out = monitor_call(MONCALL_TEST, test_in);
    if (test_out != -test_in)
	panic("MONCALL_TEST does not work");

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();
}
