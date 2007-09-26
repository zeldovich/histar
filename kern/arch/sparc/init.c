#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/part.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/uinit.h>
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

static uint32_t secret;

static uint32_t
get_secret_real(uint32_t arg)
{
    cprintf("get_secret_real: pctag=%d\n", read_pctag());
    return secret + arg;
}

static uint32_t
get_secret(uint32_t arg)
{
    return tag_call(&get_secret_real, arg);
}

static void
test_secret(void)
{
    cprintf("Trying to get secret value...\n");
    cprintf("Value + 0 = 0x%x\n", get_secret(0));
    cprintf("Value + 1 = 0x%x\n", get_secret(1));
}

void __attribute__((noreturn))
init (void)
{
    int r;

    write_tsr(TSR_T);
    mmu_init();
    bss_init();

    amba_init();
    irqmp_init();
    apbucons_init();

    page_init();
    tag_init();

    gptimer_init();
    //amba_print();

    r = greth_init();
    if (r < 0)
	cprintf("init: greth_init error: %s\n", e2s(r));
    
    kobject_init();
    sched_init();
    pstate_init();

    user_init();

    tag_set(&secret, DTAG_PTEST, sizeof(secret));
    wrtperm(PCTAG_PTEST, DTAG_PTEST, TAG_PERM_READ | TAG_PERM_WRITE);
    secret = 0xc0ffee;

    cprintf("Kernel init done, disabling trusted mode.. ");
    write_tsr(0);
    cprintf("done.\n");

    test_secret();

    cprintf("foo.\n");
    abort();

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();
}
