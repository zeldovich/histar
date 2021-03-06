#define _GNU_SOURCE 1	    // dang GNU header files

#include <machine/lnxpage.h>
#include <machine/ftglue.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <inc/error.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

struct page_info *page_infos;

//#include <ft_public.h>
//#include <ft_runtest.h>

// debug flags
enum { scrub_free_pages = 0 };
int enable_page_alloc_failure = 0;

// base of our simulated physical memory range
void *physmem_base;
int physmem_file_fd;

void
lnxpage_init(uint64_t membytes)
{
    global_npages = membytes / PGSIZE;

    char pn[256];
    sprintf(&pn[0], "/tmp/lnxpage.%d", getpid());
    physmem_file_fd = open(pn, O_RDWR | O_CREAT | O_EXCL, 0777);
    if (physmem_file_fd < 0) {
	perror("cannot create physmem file");
	exit(-1);
    }

    if (unlink(pn) < 0) {
	perror("cannot unlink physmem file");
	exit(-1);
    }

    if (ftruncate(physmem_file_fd, global_npages * PGSIZE) < 0) {
	perror("cannot resize physmem file");
	exit(-1);
    }

    uintptr_t baseptr = 0x90000000;
    physmem_base = mmap((void *) baseptr, global_npages * PGSIZE,
			PROT_READ | PROT_WRITE,
			MAP_FIXED | MAP_PRIVATE, physmem_file_fd, 0);
    if (physmem_base == MAP_FAILED) {
	perror("cannot map physmem file");
	exit(-1);
    }

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = malloc(sz);
    memset(page_infos, 0, sz);

    // chain the pages
    page_alloc_init();
    for (uint64_t i = 0; i < global_npages; i++) {
	uintptr_t fool_ft = ((uintptr_t)physmem_base) + i * PGSIZE;
	//ft_register_memory(fool_ft, PGSIZE, "physmem-page");
	void *pg = (void *) fool_ft;
	page_free(pg);
    }

    page_stats.pages_used = 0;
}

void *
pa2kva(physaddr_t pa)
{
    assert(pa < (global_npages << PGSHIFT));
    uintptr_t fool_ft = ((uintptr_t)physmem_base) + pa;
    return (void *) fool_ft;
}

physaddr_t
kva2pa(void *kva)
{
    assert(kva >= physmem_base && kva < physmem_base + (global_npages << PGSHIFT));
    return (physaddr_t) (kva - physmem_base);
}

ppn_t
pa2ppn(physaddr_t pa)
{
    assert(pa < global_npages << PGSHIFT);
    return pa >> PGSHIFT;
}

physaddr_t
ppn2pa(ppn_t pn)
{
    assert(pn <= global_npages);
    return pn << PGSHIFT;
}
