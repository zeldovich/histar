#include <kern/arch.h>
#include <kern/lib.h>
#include <machine/x86.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <inc/safeint.h>
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
    char x;
    uintptr_t p = (uintptr_t) &x;
    return p;
}

uint64_t
karch_get_tsc(void)
{
    return read_tsc();
}

void irq_arch_enable(uint32_t irqno) {}
void karch_fp_init(struct Fpregs *fpreg) {}

void pmap_set_current(struct Pagemap *pm) {}
void as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va) {}
void as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va) {}

int
check_user_access2(const void *ptr, uint64_t nbytes,
		   uint32_t reqflags, int alignbytes)
{
    /*
     * XXX
     * should really switch to copy_{to,from}_user for nacl, for performance
     */

    if (nbytes == 0)
	return 0;

    int overflow = 0;
    uintptr_t iptr = (uintptr_t) ptr;
    uintptr_t start = ROUNDDOWN(iptr, PGSIZE);
    uintptr_t end = ROUNDUP(safe_addptr(&overflow, iptr, nbytes), PGSIZE);

    if (end <= start || overflow)
	return -E_INVAL;

    for (uintptr_t va = start; va < end; va += PGSIZE) {
	if (va >= ULIM)
	    return -E_INVAL;

	int r = as_pagefault(cur_as, (void *) va, reqflags);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
thread_arch_utrap(struct Thread *t, 
		  uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_NO_MEM;
}

int
thread_arch_is_masked(const struct Thread *t)
{
    return 1;
}

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
