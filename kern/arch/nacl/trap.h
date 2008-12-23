#ifndef JOS_MACHINE_TRAP_H
#define JOS_MACHINE_TRAP_H

// Low-level trapframe jump in locore.S
void trapframe_pop(const struct Trapframe *)
    __attribute__((noreturn, regparm (1)));

void syscall_handler(void);

extern char nacl_springboard[];
extern char nacl_springboard_end[];

extern char nacl_usyscall[];
extern char nacl_usyscall_end[];

#endif
