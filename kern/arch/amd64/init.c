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
#include <kern/container.h>
#include <kern/label.h>
#include <kern/unique.h>
#include <kern/lib.h>
#include <kern/segment.h>

// XXX
#include <inc/stdio.h>

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
  cprintf ("[%p] kpanic: %s:%d: ", cur_thread, file, line);
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

void
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

void
init (void)
{
  mmu_init ();
  bss_init ();
  idt_init ();
  cons_init ();
  pic_init ();
  kclock_init ();
  pmap_init ();
  pci_init ();

  //disk_test ();

  // XXX have to alloc the container first, so that it gets ID 0
  struct Container *rc;
  assert(0 == container_alloc(&rc));

  // XXX alloc a "root fs" container as ID 1 for now
  struct Container *fsc;
  assert(0 == container_alloc(&fsc));
  assert(0 == container_put(rc, cobj_container, fsc));

  uint64_t root_handle = unique_alloc();
  struct Label *l;
  assert(0 == label_alloc(&l));

  l->lb_hdr.def_level = 1;
  assert(0 == label_set(l, root_handle, LB_LEVEL_STAR));
  assert(0 == label_copy(l, &rc->ct_hdr.label));
  assert(0 == label_copy(l, &fsc->ct_hdr.label));

  // XXX this is such a mess, but it's all going away anyway..
  struct Segment *fs_names;
  assert(0 == segment_alloc(&fs_names));
  assert(0 == label_copy(l, &fs_names->sg_hdr.label));
  assert(0 == segment_set_npages(fs_names, 1));
  assert(0 == container_put(fsc, cobj_segment, fs_names));

  int num_fs_segments = 0;
  char *fs_dir = (char*) page2kva(fs_names->sg_page[0]);
#define SEGMENT_CREATE_EMBED(name)				\
    do {							\
	extern uint8_t _binary_obj_user_##name##_start[],	\
		       _binary_obj_user_##name##_size[];	\
	int bytes = (int)_binary_obj_user_##name##_size;	\
	struct Segment *s;					\
	assert(0 == segment_alloc(&s));				\
	assert(0 == label_copy(l, &s->sg_hdr.label));		\
	assert(0 == segment_set_npages(s, bytes/PGSIZE+1));	\
	int i;							\
	for (i = 0; i < bytes; i+=PGSIZE)			\
	    memcpy(page2kva(s->sg_page[i/PGSIZE]),		\
		   &_binary_obj_user_##name##_start[i], PGSIZE);\
	i = container_put(fsc, cobj_segment, s);		\
	assert(i>0);						\
	char namebuf[64];					\
	snprintf(namebuf, 64, "%c%s", i, #name);		\
	memcpy(&fs_dir[(++num_fs_segments)*64], namebuf, 64);	\
    } while (0)

  SEGMENT_CREATE_EMBED(hello);
  SEGMENT_CREATE_EMBED(gate_test);
  SEGMENT_CREATE_EMBED(thread_test);
  SEGMENT_CREATE_EMBED(spin);
  SEGMENT_CREATE_EMBED(jmptest);
  memcpy(fs_dir, &num_fs_segments, 4);

  THREAD_CREATE_EMBED(rc, l, user_idle);
  THREAD_CREATE_EMBED(rc, l, user_shell);

  label_free(l);

  cprintf("=== kernel ready, calling schedule() ===\n");
  schedule();
}
