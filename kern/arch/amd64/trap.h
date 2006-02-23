#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H

#include <machine/mmu.h>
#include <machine/trapcodes.h>

void idt_init(void);

// Low-level trapframe jump in locore.S
void trapframe_pop(const struct Trapframe *) __attribute__((__noreturn__));

// Entry into kernel from the bootloader
void init(uint32_t start_eax, uint32_t start_ebx) __attribute__((noreturn));

// Entry into kernel from user space traps
void trap_handler(struct Trapframe *tf) __attribute__((__noreturn__));

// for profiling
extern uint64_t trap_user_iret_tsc;

#endif
