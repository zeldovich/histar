#include <kern/arch.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <stdlib.h>
#include <stdio.h>

char boot_cmdline[256];

int
page_map_alloc(struct Pagemap **pm_store)
{
    *pm_store = malloc(sizeof(**pm_store));
    return 0;
}

void
page_map_free(struct Pagemap *pgmap)
{
    free(pgmap);
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first,
		  const void *last, int create,
		  page_map_traverse_cb cb, const void *arg)
{
    return -E_NO_MEM;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
    return -E_NO_MEM;
}

ppn_t
pa2ppn(physaddr_t pa)
{
    return pa >> PGSHIFT;
}

physaddr_t
ppn2pa(ppn_t pn)
{
    return pn << PGSHIFT;
}

void __attribute__((noreturn))
machine_reboot(void)
{
    printf("machine_reboot()\n");
    exit(0);
}

uintptr_t
karch_get_sp(void)
{
    int x;
    uintptr_t addr = (uintptr_t) &x;
    return addr;
}

uint64_t
karch_get_tsc(void)
{
    return 0;
}

void irq_arch_enable(uint32_t irqno) {}
void karch_fp_init(struct Fpregs *fpreg) {}

void pmap_set_current(struct Pagemap *pm) {}
void as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va) {}
void as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va) {}
void as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va) {}

int
as_arch_putpage(struct Pagemap *pmap, void *va, void *pp, uint32_t flags)
{
    return -E_NO_MEM;
}

int
check_user_access2(const void *ptr, uint64_t nbytes,
		   uint32_t reqflags, int alignbytes)
{
    return 0;
}

int
thread_arch_utrap(struct Thread *t, 
		  uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_NO_MEM;
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    return -E_INVAL;
}

int
thread_arch_is_masked(const struct Thread *t)
{
    return 1;
}

void thread_arch_jump(struct Thread *t, const struct thread_entry *te) {}

void
jos_longjmp(volatile struct jos_jmp_buf *buf, int val)
{
    struct jos_jmp_buf *b = (struct jos_jmp_buf *) buf;
    siglongjmp(b->native_jb, val ?: 1);
}

/*
 * Yow!  glibc/sysdeps/unix/sysv/linux/arch/sysdep.h
 */
#ifdef __x86_64__
#define PTR_MANGLE(x) __asm("xorq %%fs:0x30, %0; rolq $0x11, %0" : "+r" (x))
#endif

#ifdef __i386__
#define PTR_MANGLE(x) __asm("xorl %%gs:0x18, %0; roll $0x09, %0" : "+r" (x))
#endif

void
karch_jmpbuf_init(struct jos_jmp_buf *jb, void *fn, void *stackbase)
{
    uintptr_t pc = (uintptr_t) fn;
    uintptr_t sp = ROUNDUP((uintptr_t) stackbase, PGSIZE);

    PTR_MANGLE(pc);
    PTR_MANGLE(sp);

    int nregs = sizeof(jb->native_jb[0].__jmpbuf) /
		sizeof(jb->native_jb[0].__jmpbuf[0]);

    jb->native_jb[0].__jmpbuf[nregs - 1] = pc;
    jb->native_jb[0].__jmpbuf[nregs - 2] = sp;
    jb->native_jb[0].__mask_was_saved = 0;
}

void
thread_arch_rebooting(struct Thread *t)
{
}
