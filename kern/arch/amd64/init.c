#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/multiboot.h>
#include <machine/boot.h>
#include <machine/tsctimer.h>
#include <machine/mp.h>
#include <dev/sercons.h>
#include <dev/cgacons.h>
#include <dev/lptcons.h>
#include <dev/pci.h>
#include <dev/picirq.h>
#include <dev/kclock.h>
#include <dev/apic.h>
#include <dev/acpi.h>
#include <dev/vesafb.h>
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

uint64_t ap_stacktop;

static void
flush_tlb_hard (void)
{
  uint64_t cr3 = rcr3();
  uint64_t cr4 = rcr4();

  lcr4(cr4 & ~CAST64 (CR4_PGE));
  lcr3(cr3);
  lcr4(cr4);
}

static void
bss_init (void)
{
  extern char edata[], end[];
  memset (edata, 0, end - edata);
}

static void
seg_init (void)
{
    uint32_t i = arch_cpu();
    
    /* Move gdt to kernel memory and reload */
    gdtdesc[i].pd_base = (uintptr_t) &gdt[i];
    lgdt(&gdtdesc[i].pd_lim);
    
    /* Load TSS */
    gdt[i][(GD_TSS >> 3)] = (SEG_TSSA | SEG_P | SEG_A | SEG_BASELO (&tss[i])
			  | SEG_LIM (sizeof (tss[i]) - 1));
    gdt[i][(GD_TSS >> 3) + 1] = SEG_BASEHI (&tss[i]);
    ltr(GD_TSS);
}

static void
bootothers(void)
{
    extern uint8_t _binary_boot_bootother_start[],
	_binary_boot_bootother_size[], start_ap[];
    uint64_t size = (uint64_t) _binary_boot_bootother_size;

    if (size > PGSIZE - 4)
	panic("bootother too big: %lu\n", size);

    uint8_t *code = pa2kva(APBOOTSTRAP);
    memcpy(code, _binary_boot_bootother_start, size);

    for (struct cpu * c = cpus; c < cpus + ncpu; c++) {
	if (c == cpus + arch_cpu())
	    continue;

	ap_stacktop = KSTACKTOP(c - cpus);
	// Pass the %eip for the 32-bit bootstrap code
	*(uint32_t *) (code + 4092) = (uint32_t) RELOC(start_ap);
	apic_start_ap(c->apicid, APBOOTSTRAP);

	while (c->booted == 0) ;
    }
    page_free(code);
}

void __attribute__((noreturn))
init_ap(void)
{
    seg_init();
    lidt(&idtdesc.pd_lim);
    apic_init();
    
    mfence();
    cpus[arch_cpu()].booted = 1;
    
    while(!bcpu->booted);
    flush_tlb_hard ();

    for (;;);
}

void __attribute__((noreturn))
init (uint32_t start_eax, uint32_t start_ebx)
{
    seg_init();
    bss_init();

    uint64_t lower_kb = 0;
    uint64_t upper_kb = 0;
    static struct sysx_info sxi;

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
	memcpy(&sxi, pa2kva(start_ebx), sizeof(sxi));
	char *cmdline = pa2kva(sxi.cmdline);
	strncpy(&boot_cmdline[0], cmdline, sizeof(boot_cmdline) - 1);
	upper_kb = sxi.extmem_kb;

	if (sxi.vbe_mode)
	    vesafb_init(pa2kva(sxi.vbe_control_info),
			pa2kva(sxi.vbe_mode_info),
			sxi.vbe_mode);
    }

    idt_init();
    cgacons_init();
    sercons_init();
    lptcons_init();
    mp_init();
    pic_init();
    
    acpi_init();	/* Picks up HPET, PM timer */
    apic_init();	/* LAPIC timer for preemption */
    tsc_timer_init();	/* Optimization for PM timer */
    pit_init();		/* Fallback position */

    page_init(lower_kb, upper_kb, sxi.e820_map, sxi.e820_nents);
    pci_init();
    part_init();

    kobject_init();
    sched_init();
    pstate_init();

    bootothers();

    /* Nuke identically mapped physical memory */
    bootpml4.pm_ent[0] = 0;
    flush_tlb_hard ();
    bcpu->booted = 1;
    
    prof_init();
    if (strstr(&boot_cmdline[0], "reboot=periodic")) {
	static struct periodic_task reboot_pt =
	    { .pt_fn = &machine_reboot, .pt_interval_msec = 3600 * 1000 };
	timer_add_periodic(&reboot_pt);
    }

    user_init();

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();
}
