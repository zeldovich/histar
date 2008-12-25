#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#ifndef __ASSEMBLER__
#include <inc/thread.h>
#include <kern/arch/amd64/mmu-x86.h>
#endif

#define PGSHIFT 12
#define PGSIZE (1 << PGSHIFT)

/* Page fault error codes */
#define FEC_P 0x1	    /* Fault caused by protection violation */
#define FEC_W 0x2		/* Fault caused by a write */
#define FEC_U 0x4		/* Fault occured in user mode */
#define FEC_RSV 0x8		/* Fault caused by reserved PTE bit */
#define FEC_I 0x10		/* Fault caused by instruction fetch */

#define FL_TF 0x00000100      /* Trap Flag */

#ifndef __ASSEMBLER__
#include <inc/thread.h>

struct Trapframe_aux {
  struct thread_entry_args tfa_entry_args;
};

struct Trapframe {
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

struct farptr {
    uint32_t fp_reg;
    uint16_t fp_sel;
} __attribute__((packed));

#endif

#endif
