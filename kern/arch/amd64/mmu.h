#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#ifndef __ASSEMBLER__
# include <inc/types.h>
# include <inc/intmacro.h>
#else /* __ASSEMBLER__ */
# define UINT64(x) x
# define CAST64(x) (x)
#endif /* __ASSEMBLER__ */
#define ONE UINT64 (1)

#include <machine/mmu-x86.h>

/*
 * AMD64-specific bits
 */

/* Page directory and page table constants. */
#define NPTENTRIES 512		/* page table entries per page table */

#ifndef __ASSEMBLER__
struct Trapframe {
  /* callee-saved registers except %rax and %rsi */
  uint64_t tf_rcx;
  uint64_t tf_rdx;
  uint64_t tf_rdi;
  uint64_t tf_r8;
  uint64_t tf_r9;
  uint64_t tf_r10;
  uint64_t tf_r11;

  /* caller-saved registers */
  uint64_t tf_rbx;
  uint64_t tf_rbp;
  uint64_t tf_r12;
  uint64_t tf_r13;
  uint64_t tf_r14;
  uint64_t tf_r15;

  /* for use by trap_{ec,noec}_entry_stub */
  union {
    uint64_t tf_rsi;
    uint64_t tf__trapentry_rip;
  };

  /* saved by trap_{ec,noec}_entry_stub */
  uint64_t tf_rax;

  /* hardware-saved registers */
  uint32_t tf_err;
  uint32_t tf__pad1;
  uint64_t tf_rip;
  uint16_t tf_cs;
  uint16_t tf_ds;	// not saved/restored by hardware
  uint16_t tf_es;	// not saved/restored by hardware
  uint16_t tf_fs;	// not saved/restored by hardware
  uint64_t tf_rflags;
  uint64_t tf_rsp;
  uint16_t tf_ss;
  uint16_t tf_gs;	// not saved/restored by hardware
  uint16_t tf__pad3[2];
};
#endif

#endif /* !JOS_INC_MMU_H */
