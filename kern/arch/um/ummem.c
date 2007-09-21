#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <machine/um.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <stdlib.h>
#include <stdio.h>

static void *mem_base;
static uint64_t mem_bytes;
struct page_info *page_infos;

void *
pa2kva(physaddr_t pa)
{
    return mem_base + pa;
}

physaddr_t
kva2pa(void *kva)
{
    physaddr_t addr = (physaddr_t) kva;
    physaddr_t base = (physaddr_t) mem_base;
    assert(addr >= base && addr < base + mem_bytes);
    return addr - base;
}

void
um_mem_init(uint64_t bytes)
{
    cprintf("%"PRIu64" bytes physical memory\n", bytes);

    mem_bytes = bytes;
    mem_base = malloc(bytes + PGSIZE);
    mem_base = ROUNDUP(mem_base, PGSIZE);
    assert(mem_base);

    page_infos = malloc((mem_bytes / PGSIZE) * sizeof(*page_infos));

    page_alloc_init();
    for (uint64_t pg = 0; pg < mem_bytes / PGSIZE; pg++) {
	void *p = mem_base + pg * PGSIZE;
	page_free(p);
    }
}
