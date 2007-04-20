#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#ifndef __ASSEMBLER__
# include <inc/types.h>
# include <inc/intmacro.h>
# include <inc/thread.h>
# define CASTPTR(x) ((uintptr_t) x)
#else /* __ASSEMBLER__ */
# define UINT64(x) x
# define CAST64(x) (x)
# define CASTPTR(x) (x)
#endif /* __ASSEMBLER__ */
#define ONE UINT64 (1)

#include <kern/arch/amd64/mmu-x86.h>

/*
 * i386-specific bits
 */

/* Page directory and page table constants. */
#define NPTBITS	    10		/* log2(NPTENTRIES) */
#define NPTLVLS	    1		/* page table depth -1 */
#define PD_SKIP	    2		/* Offset of pd_lim in Pseudodesc */

#ifndef __ASSEMBLER__
/* Pseudo-descriptors used for LGDT, LLDT and LIDT instructions. */
struct Pseudodesc {
  uint16_t pd__garbage;
  uint16_t pd_lim;		/* Limit */
  uint32_t pd_base;		/* Base address */
} __attribute__((packed));

struct Tss {
  uint16_t tss_prevtask;
  uint16_t tss_pad0;
  struct {
    uint32_t esp;
    uint16_t ss;
    uint16_t pad;
  } tss_sp[3];			/* Stack pointer for CPL 0, 1, 2 */

  uint32_t tss_cr3;
  uint32_t tss_eip;
  uint32_t tss_eflags;
  uint32_t tss_eax;
  uint32_t tss_ecx;
  uint32_t tss_edx;
  uint32_t tss_ebx;
  uint32_t tss_esp;
  uint32_t tss_ebp;
  uint32_t tss_esi;
  uint32_t tss_edi;

  uint16_t tss_es;
  uint16_t tss_pad1;
  uint16_t tss_cs;
  uint16_t tss_pad2;
  uint16_t tss_ss;
  uint16_t tss_pad3;
  uint16_t tss_ds;
  uint16_t tss_pad4;
  uint16_t tss_fs;
  uint16_t tss_pad5;
  uint16_t tss_gs;
  uint16_t tss_pad6;

  uint16_t tss_ldt;
  uint16_t tss_pad7;

  uint16_t tss_debug;
  uint16_t tss_iomb;
  uint8_t tss_iopb[];
} __attribute__ ((packed));

struct Gatedesc {
  uint64_t gd;
};

struct Trapframe_aux {
  struct thread_entry_args tfa_entry_args;
};

struct Trapframe {
  /* XXX not laid out properly yet */
  uint32_t tf_ebx;
  uint32_t tf_ecx;
  uint32_t tf_edi;
  uint32_t tf_esi;
  uint32_t tf_ebp;

  uint16_t tf_ds;
  uint16_t tf_es;

  /* for use by trap_{ec,noec}_entry_stub */
  union {
    uint32_t tf_edx;
    uint32_t tf__trapentry_eip;
  };

  /* saved by trap_{ec,noec}_entry_stub */
  uint32_t tf_eax;

  /* hardware-saved registers */
  uint32_t tf_err;
  uint32_t tf_eip;
  uint16_t tf_cs;
  uint16_t tf_fs;	// not saved/restored by hardware
  uint32_t tf_eflags;
  uint32_t tf_esp;
  uint16_t tf_ss;
  uint16_t tf_gs;	// not saved/restored by hardware
};
#endif

#endif /* !JOS_MACHINE_MMU_H */
