#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H

#include <machine/mmu.h>
#include <machine/trapcodes.h>

void idt_init();

// Low-level trapframe jump in locore.S
void trapframe_pop(struct Trapframe *);

// For following user pointers in kernel-space
extern int page_fault_mode;
#define	PFM_NONE    0
#define PFM_KILL    1

#endif
