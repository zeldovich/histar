#ifndef JOS_INC_ARCH_H
#define JOS_INC_ARCH_H

uint64_t arch_read_tsc(void);
void arch_fpregs_save(void *fpregs);
void arch_fpregs_restore(void *fpregs);

#endif
