#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/part.h>
#include <machine/trap.h>
#include <machine/srmmu.h>
#include <machine/pmap.h>
#include <dev/apbucons.h>
#include <dev/amba.h>
#include <dev/irqmp.h>
#include <dev/gptimer.h>

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
    bss_init();
    
    amba_init();
    apbucons_init();
    amba_print();

    irqmp_init();    
    gptimer_init();

    page_init();
    
    cprintf("hello from sparc init\n");
    for (;;) { }
}
