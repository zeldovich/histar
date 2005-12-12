#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H

#include <machine/mmu.h>
#include <machine/trapcodes.h>

void idt_init(void);

// Low-level trapframe jump in locore.S
void trapframe_pop(struct Trapframe *) __attribute__((__noreturn__));

#endif
