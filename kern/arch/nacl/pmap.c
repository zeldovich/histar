#include <machine/nacl.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

void __attribute__((noreturn))
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    assert(0);
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first,
		  const void *last, int create,
		  page_map_traverse_cb cb, const void *arg)
{
    if (last > (const void *) ULIM)
	last = (const void *) ULIM;
    
    if (cb == as_arch_page_invalidate_cb) {
	// TLB flush...
	if (last > first && munmap((void *)first, last - first) < 0)
	    cprintf("%s:%d: munmap(%p, %d) failed: %s\n",
		    __FILE__, __LINE__, first, last-first, strerror(errno));
	//XXX pagetree_pin
	return 0;
    } else {
	assert(0);
    }
    return -E_NO_MEM;
}

int
as_arch_putpage(struct Pagemap *pmap, void *va, void *pp, uint32_t flags)
{
    assert(va < (void *)ULIM);
    int prot = PROT_READ;

    if ((flags & SEGMAP_WRITE))
	prot |= PROT_WRITE;

    // XXX need to check code
    if ((flags & SEGMAP_EXEC))
	prot |= PROT_EXEC;

    //XXX pagetree_pin
    return nacl_mmap(va, pp, PGSIZE, prot);
}
