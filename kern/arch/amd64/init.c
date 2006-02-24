#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/thread.h>
#include <machine/trap.h>
#include <dev/console.h>
#include <dev/disk.h>
#include <dev/pci.h>
#include <dev/picirq.h>
#include <dev/kclock.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/uinit.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/prof.h>

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

#if 0
  mon_backtrace (0, 0, 0);

dead:
  /* break into the kernel monitor */
  while (1)
    monitor (NULL);
#else
 dead:
  abort ();
#endif
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
  uint64_t cr3;
  uint64_t cr4;
  __asm volatile ("movq %%cr3,%0":"=a" (cr3));
  __asm volatile ("movq %%cr4,%0":"=a" (cr4));
  __asm volatile ("movq %0,%%cr4"::"a" (cr4 & ~CAST64 (CR4_PGE)));
  __asm volatile ("movq %0,%%cr3"::"a" (cr3));
  __asm volatile ("movq %0,%%cr4"::"a" (cr4));
}

static void
mmu_init (void)
{
  /* Move gdt to kernel memory and reload */
  gdtdesc.pd_base = (uintptr_t) & gdt;
  __asm volatile ("lgdt (%0)"::"r" (&gdtdesc.pd_lim));

  /* Nuke identically mapped physical memory */
  bootpml4.pm_ent[0] = 0;
  flush_tlb_hard ();

  /* Load TSS */
  gdt[(GD_TSS >> 3)] = (SEG_TSSA | SEG_P | SEG_BASELO (&tss)
			| SEG_LIM (sizeof (tss) - 1));
  gdt[(GD_TSS >> 3) + 1] = SEG_BASEHI (&tss);
  __asm volatile ("ltr %w0"::"r" (GD_TSS));
}

static void
bss_init (void)
{
  extern char edata[], end[];
  memset (edata, 0, end - edata);
}

void __attribute__((noreturn))
init (uint32_t start_eax, uint32_t start_ebx)
{
    mmu_init();
    bss_init();

    struct multiboot_info *mbi = 0;
    if (start_eax == MULTIBOOT_EAX_MAGIC)
	mbi = (struct multiboot_info *) pa2kva(start_ebx);

    if (mbi && (mbi->flags & MULTIBOOT_INFO_CMDLINE)) {
	char *cmdline = pa2kva(mbi->cmdline);
	strncpy(&boot_cmdline[0], cmdline, sizeof(boot_cmdline) - 1);
    }

    idt_init();
    cons_init();
    pic_init();
    kclock_init();
    timer_init();
    pmap_init(mbi);
    pci_init();

    kobject_init ();
    sched_init ();
    pstate_init ();

    prof_init();

    user_init ();

    cprintf("=== kernel ready, calling schedule() ===\n");
    schedule();
    thread_run(cur_thread);
}
