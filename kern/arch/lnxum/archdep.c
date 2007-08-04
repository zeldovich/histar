#include <kern/arch.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <stdlib.h>
#include <stdio.h>

char boot_cmdline[256];
struct page_info *page_infos;

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

void *
pa2kva(physaddr_t pa)
{
    return (void *) pa;
}

physaddr_t
kva2pa(void *kva)
{
    return (physaddr_t) kva;
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
    return 0;
}

uint64_t
karch_get_tsc(void)
{
    return 0;
}

void karch_jmpbuf_init(struct jos_jmp_buf *jb, void *fn, void *stackbase) {}
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

void
thread_arch_run(const struct Thread *t)
{
    printf("thread_arch_run: what now?\n");
    exit(0);
}

void
thread_arch_idle(void)
{
    printf("thread_arch_idle\n");
    exit(0);
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
jos_longjmp(struct jos_jmp_buf *buf, int val)
{
    printf("jos_longjmp: not supported\n");
    exit(1);
}

int
jos_setjmp(struct jos_jmp_buf *buf)
{
    return 0;
}
