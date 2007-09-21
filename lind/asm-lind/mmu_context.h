#ifndef __LIND_MMU_CONTEXT_H
#define __LIND_MMU_CONTEXT_H

#include <asm/desc.h>
#include <asm/atomic.h>
#include <asm/pgalloc.h>
#include <asm/pda.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#define get_mmu_context(task) do ; while(0)
#define activate_context(tsk) do ; while(0)

#define deactivate_mm(tsk,mm)	do { } while (0)

static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
    return 0;
}

#define destroy_context(mm)		do { } while(0)

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, 
			     struct task_struct *tsk)
{
    panic("switch_mm: not implemented");
}

static inline void activate_mm(struct mm_struct *old, struct mm_struct *new)
{
    panic("activate_mm: not implemented");
}

#endif
