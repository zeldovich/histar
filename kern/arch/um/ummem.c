#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <machine/um.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <stdlib.h>
#include <stdio.h>

void *um_mem_base;
uint64_t um_mem_bytes;
struct page_info *page_infos;

void
um_mem_init(uint64_t bytes)
{
    cprintf("%"PRIu64" bytes physical memory\n", bytes);

    um_mem_bytes = bytes;
    um_mem_base = malloc(bytes + PGSIZE);
    um_mem_base = ROUNDUP(um_mem_base, PGSIZE);
    assert(um_mem_base);

    page_infos = malloc((um_mem_bytes / PGSIZE) * sizeof(*page_infos));

    page_alloc_init();
    for (uint64_t pg = 0; pg < um_mem_bytes / PGSIZE; pg++) {
	void *p = um_mem_base + pg * PGSIZE;
	page_free(p);
    }
}
