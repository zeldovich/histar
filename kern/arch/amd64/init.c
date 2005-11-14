
#include <machine/pmap.h>
#include <machine/x86.h>
#include <kern/lib.h>
#include <dev/console.h>
#include <dev/disk.h>

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
#if 0
  cprintf ("[%x] kpanic: %s:%d: ",
	   (curenv ? curenv->env_id : 0), file, line);
#else
  cprintf ("kpanic: %s:%d: ", file, line);
#endif
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
  for (;;);
}

void
flush_tlb_hard (void)
{
  uint64_t cr3;
  uint64_t cr4;
  asm volatile ("movq %%cr3,%0":"=a" (cr3));
  asm volatile ("movq %%cr4,%0":"=a" (cr4));
  asm volatile ("movq %0,%%cr4"::"a" (cr4 & ~CAST64 (CR4_PGE)));
  asm volatile ("movq %0,%%cr3"::"a" (cr3));
  asm volatile ("movq %0,%%cr4"::"a" (cr4));
}

static void
mmu_init (void)
{
  /* Move gdt to kernel memory and reload */
  gdtdesc.pd_base = (uintptr_t) & gdt;
  asm volatile ("lgdt (%0)"::"r" (&gdtdesc.pd_lim));

  /* Nuke identically mapped physical memory */
  bootpml4[0] = 0;
  flush_tlb_hard ();

  /* Load TSS */
  gdt[(GD_TSS >> 3)] = (SEG_TSSA | SEG_P | SEG_BASELO (&tss)
			| SEG_LIM (sizeof (tss) - 1));
  gdt[(GD_TSS >> 3) + 1] = SEG_BASEHI (&tss);
  asm volatile ("ltr %w0"::"r" (GD_TSS));
}

static void
bss_init (void)
{
  extern char edata[], end[];
  memset (edata, 0, end - edata);
}

static void
idt_init (void)
{
  extern char Xunknownec[];
  extern char Xunknown[];
  int i;
  for (i = 0; i < 0x100; i++)
    SETGATE (idt[i], SEG_TG, GD_KT, Xunknown, 0);
  SETGATE (idt[8], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[10], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[11], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[12], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[13], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[14], SEG_TG, GD_KT, Xunknownec, 0);
  SETGATE (idt[17], SEG_TG, GD_KT, Xunknownec, 0);

  SETGATE (idt[48], SEG_TG, GD_KT, Xunknown, 3);
}

void
init (void)
{
  mmu_init ();
  bss_init ();
  idt_init ();
  cons_init ();

  //pmap_init ();

  cprintf ("Hello world\n");

  disk_init ();
  disk_test ();

  abort ();
}
