#ifndef JOS_MACHINE_TRAP
#define JOS_MACHINE_TRAP

void exception_handler(int trapcode, struct Trapframe *, uint32_t);
void trapframe_pop(const struct Trapframe *) __attribute__((__noreturn__));

#endif /* !JOS_MACHINE_TRAP */
