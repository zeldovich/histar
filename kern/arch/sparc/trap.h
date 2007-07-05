#ifndef JOS_MACHINE_TRAP_H
#define JOS_MACHINE_TRAP_H

// Entry into kernel from the bootloader
void init(void) __attribute__((noreturn));

// Entry into kernel from user space traps
void trap_handler(struct Trapframe *tf) __attribute__((__noreturn__));

#endif
