#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#define	PGSHIFT		12
#define	PGSIZE		(1 << PGSHIFT)
#define PGMASK		(PGSIZE - 1)
#define PGOFF(la)	(((uintptr_t) (la)) & PGMASK)

#define PTE_ADDR(e)	((e) & ~PGMASK)

struct Trapframe {
};

struct Trapframe_aux {
};

struct Fpregs {
};

#endif
