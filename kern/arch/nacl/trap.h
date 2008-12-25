#ifndef JOS_MACHINE_TRAP_H
#define JOS_MACHINE_TRAP_H

// Low-level trapframe jump in locore.S
void trapframe_pop(const struct Trapframe *)
     __attribute__((noreturn, regparm (1)));

#endif
