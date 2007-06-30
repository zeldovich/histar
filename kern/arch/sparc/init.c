#include <kern/arch.h>
#include <kern/lib.h>
#include <machine/trap.h>
#include <machine/srmmu.h>
#include <dev/leonsercons.h>
#include <dev/amba.h>

char boot_cmdline[256];

static void
mmu_init (void)
{
    uint32_t ctrl = srmmu_get_mmureg();
    /* make sure No Fault and Enable are clear */
    ctrl &= ~(SRMMU_CTRL_E | SRMMU_CTRL_NF);
    srmmu_set_mmureg(ctrl);
    /* clear Context Table Pointer and Context */
    srmmu_set_ctable_ptr(0);
    srmmu_set_context(0);
}

static void
bss_init (void)
{
    extern char edata[], end[];
    memset (edata, 0, end - edata);
}

void __attribute__((noreturn))
init (void)
{
    mmu_init();
    bss_init();

    amba_init();
    cprintf("hello from sparc init\n");
    for (;;) { }
}
