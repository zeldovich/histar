#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/multiboot.h>
#include <machine/boot.h>
#include <dev/console.h>
#include <dev/disk.h>
#include <dev/pci.h>
#include <dev/picirq.h>
#include <dev/kclock.h>
#include <dev/apic.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/uinit.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/thread.h>
#include <kern/arch.h>

char boot_cmdline[256];

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
static const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic (const char *file, int line, const char *fmt, ...)
{
  va_list ap;

  if (panicstr)
    goto dead;
  panicstr = fmt;

  va_start (ap, fmt);
  cprintf ("[%ld] kpanic: %s:%d: ",
	   cur_thread ? cur_thread->th_ko.ko_id : 0,
	   file, line);
  vcprintf (fmt, ap);
  cprintf ("\n");
  va_end (ap);

 dead:
  abort ();
}

void
abort (void)
{
  outw (0x8A00, 0x8A00);
  outw (0x8A00, 0x8AE0);
  for (;;)
    ;
}

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
mmu_init (void)
{
  /* Move gdt to kernel memory and reload */
  gdtdesc.pd_base = (uintptr_t) &gdt;
  lgdt(&gdtdesc.pd_lim);

  /* Nuke identically mapped physical memory */
  bootpml4.pm_ent[0] = 0;
  flush_tlb_hard ();

  /* Load TSS */
  gdt[(GD_TSS >> 3)] = (SEG_TSSA | SEG_P | SEG_A | SEG_BASELO (&tss)
			| SEG_LIM (sizeof (tss) - 1));
  gdt[(GD_TSS >> 3) + 1] = SEG_BASEHI (&tss);
  ltr(GD_TSS);
}

static void
bss_init (void)
{
  extern char edata[], end[];
  memset (edata, 0, end - edata);
}

static void
reboot_periodic(void)
{
    // periodic tasks get called first right away,
    // and then after a period of time
    static int counter = 0;
    if (counter++)
	machine_reboot();
}

void __attribute__((noreturn))
init (uint32_t start_eax, uint32_t start_ebx)
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

    // Our boot sector passes in the upper memory size this way
    if (start_eax == DIRECT_BOOT_EAX_MAGIC)
	upper_kb = start_ebx;

    idt_init();
    cons_init();
    pic_init();
    //apic_init();
    kclock_init();
    timer_init();
    page_init(lower_kb, upper_kb);
    pci_init();

    kobject_init ();
    sched_init ();
    pstate_init ();

    prof_init();
    if (strstr(&boot_cmdline[0], "reboot=periodic")) {
	static struct periodic_task reboot_pt = { .pt_fn = &reboot_periodic };
	reboot_pt.pt_interval_ticks = 3600 * kclock_hz;
	timer_add_periodic(&reboot_pt);
    }

    user_init ();

    cprintf("=== kernel ready, calling schedule() ===\n");
    schedule();
    thread_run(cur_thread);
}
