#ifndef JOS_MACHINE_TRAP_H
#define JOS_MACHINE_TRAP_H

#include <machine/mmu.h>
#include <machine/trapcodes.h>

void idt_init(void);

// Low-level trapframe jump in locore.S
void trapframe_pop(const struct Trapframe *)
    __attribute__((noreturn, regparm (1)));

// Idle with interrupts enabled
void thread_arch_idle_asm(void)
    __attribute__((noreturn));

// Entry into kernel from the bootloader
void init(uint32_t start_eax, uint32_t start_ebx)
    __attribute__((noreturn, regparm (2)));

// Entry into kernel from user space traps
void trap_handler(struct Trapframe *tf, uint32_t trampoline_eip)
    __attribute__((noreturn, regparm (2)));

#endif
