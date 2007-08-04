#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#define PGSHIFT 12
#define PGSIZE (1 << PGSHIFT)
#define PGOFF(la) (((uintptr_t) (la)) & 0xFFF)
#define PTE_ADDR(pte) 0

struct Trapframe {
};

struct Trapframe_aux {
};

struct Fpregs {
};

#endif
