#ifndef JOS_MACHINE_TRAP_H
#define JOS_MACHINE_TRAP_H

// Entry into kernel from the bootloader
void init(void) __attribute__((noreturn));

// Entry into kernel from user space traps
void trap_handler(struct Trapframe *tf, uint32_t tbr) 
     __attribute__((__noreturn__));

// Low-level trapframe jump in locore.S
void trapframe_pop(const struct Trapframe *) __attribute__((__noreturn__));

// Idle with interrupts enabled
void thread_arch_idle_asm(void) __attribute__((noreturn));

#endif
