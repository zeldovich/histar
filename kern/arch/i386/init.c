#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/multiboot.h>
#include <machine/boot.h>
#include <machine/tsctimer.h>
#include <dev/lptcons.h>
#include <dev/sercons.h>
#include <dev/cgacons.h>
#include <dev/disk.h>
#include <dev/pci.h>
#include <dev/picirq.h>
#include <dev/kclock.h>
#include <dev/apic.h>
#include <dev/acpi.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/uinit.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/part.h>

char boot_cmdline[256];

static void
flush_tlb_hard(void)
{
    uint32_t cr3 = rcr3();
    uint32_t cr4 = rcr4();

    lcr4(cr4 & ~CR4_PGE);
    lcr3(cr3);
    lcr4(cr4);
}

static void
mmu_init(void)
{
    /* Move gdt to kernel memory and reload */
    gdtdesc.pd_base = (uintptr_t) &gdt;
    lgdt(&gdtdesc.pd_lim);

    /* Nuke identically mapped physical memory */
    for (uint32_t i = 0; i < 256; i++)
	bootpd.pm_ent[i] = 0;
    flush_tlb_hard();

    /* Load TSS */
    gdt[(GD_TSS >> 3)] = SEG_BASELO(&tss) | SEG_LIM(sizeof(tss) - 1) |
			 SEG_P | SEG_A | SEG_TSSA;
    ltr(GD_TSS);
}

static void
bss_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
}

void __attribute__((noreturn, regparm (2)))
init(uint32_t start_eax, uint32_t start_ebx)
{
    mmu_init();
    bss_init();

    uint64_t lower_kb = 0;
    uint64_t upper_kb = 0;

    if (start_eax == MULTIBOOT_EAX_MAGIC) {
	struct multiboot_info *mbi = (struct multiboot_info *) pa2kva(start_ebx);

	if ((mbi->flags & MULTIBOOT_INFO_CMDLINE)) {
	    char *cmdline = pa2kva(mbi->cmdline);
	    strncpy(&boot_cmdline[0], cmdline, sizeof(boot_cmdline) - 1);
	}

	if ((mbi->flags & MULTIBOOT_INFO_MEMORY)) {
	    lower_kb = mbi->mem_lower;
	    upper_kb = mbi->mem_upper;
	}
    }

    if (start_eax == SYSXBOOT_EAX_MAGIC) {
	struct sysx_info *sxi = pa2kva(start_ebx);
	char *cmdline = pa2kva(sxi->cmdline);
	strncpy(&boot_cmdline[0], cmdline, sizeof(boot_cmdline) - 1);
	upper_kb = sxi->extmem_kb;
    }

    // Our boot sector passes in the upper memory size this way
    if (start_eax == DIRECT_BOOT_EAX_MAGIC)
	upper_kb = start_ebx;

    idt_init();
    cgacons_init();
    sercons_init();
    lptcons_init();
    pic_init();

    acpi_init();	/* Picks up HPET, PM timer */
    apic_init();	/* LAPIC timer for preemption */
    tsc_timer_init();	/* Optimization for PM timer */
    pit_init();		/* Fallback position */

    page_init(lower_kb, upper_kb);
    pci_init();
    part_init();

    kobject_init();
    sched_init();
    pstate_init();

    prof_init();
    if (strstr(&boot_cmdline[0], "reboot=periodic")) {
	static struct periodic_task reboot_pt =
	    { .pt_fn = &machine_reboot, .pt_interval_sec = 3600 };
	timer_add_periodic(&reboot_pt);
    }

    user_init();

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();
}
