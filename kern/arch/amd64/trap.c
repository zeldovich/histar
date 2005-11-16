
#include "kern/lib.h"
#include "machine/mmu.h"

void
unknownec (struct Trapframe *tfp, int trapno)
{
  cprintf ("unknown trapec\n");
  abort ();
}

void
unknown (struct Trapframe *tfp, int trapno)
{
  cprintf ("unknown trap\n");
  abort ();
}


static void __attribute__((__unused__))
trap_field_symbols (void)
{
#define TF_DEF(field)							\
  asm volatile (".globl\t" #field "\n\t.set\t" #field ",%0"		\
		:: "m" (*(int *) offsetof (struct Trapframe, field)))
  TF_DEF (tf_rax);
  TF_DEF (tf_rcx);
  TF_DEF (tf_rdx);
  TF_DEF (tf_rsi);
  TF_DEF (tf_rdi);
  TF_DEF (tf_r8);
  TF_DEF (tf_r9);
  TF_DEF (tf_r10);
  TF_DEF (tf_r11);
  TF_DEF (tf_rbx);
  TF_DEF (tf_rbp);
  TF_DEF (tf_r12);
  TF_DEF (tf_r13);
  TF_DEF (tf_r14);
  TF_DEF (tf_r15);
  TF_DEF (tf_err);
  TF_DEF (tf_rip);
  TF_DEF (tf_cs);
  TF_DEF (tf_rflags);
  TF_DEF (tf_rsp);
  TF_DEF (tf_ss);

#define TF_NEG_DEF(field)						  \
  asm volatile (".globl\t" #field "_neg\n\t.set\t" #field "_neg,%0"	  \
		:: "m" (*(int *) (sizeof (struct Trapframe)		  \
				  - offsetof (struct Trapframe, field))))
  TF_NEG_DEF (tf_err);
  TF_NEG_DEF (tf_rip);

  asm volatile (".globl\ttf_size\n\t.set\ttf_size,%0"
		:: "m" (*(int *) sizeof (struct Trapframe)));
}

