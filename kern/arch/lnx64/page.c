#define _GNU_SOURCE 1	    // dang GNU header files

#include <machine/lnxpage.h>
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

size_t global_npages;
struct page_stats page_stats;
struct page_info *page_infos;

// debug flags
static int scrub_free_pages = 0;

// linked list of pages
struct Page_link {
    TAILQ_ENTRY(Page_link) pp_link;     // free list link
};
static TAILQ_HEAD(Page_list, Page_link) page_free_list;

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

    physmem_base = mmap(0, global_npages * PGSIZE,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, physmem_file_fd, 0);
    if (physmem_base == MAP_FAILED) {
	perror("cannot map physmem file");
	exit(-1);
    }

    // Allocate space for page status info.
    uint64_t sz = global_npages * sizeof(*page_infos);
    page_infos = malloc(sz);
    memset(page_infos, 0, sz);

    // chain the pages
    TAILQ_INIT(&page_free_list);
    for (uint64_t i = 0; i < global_npages; i++) {
	void *pg = physmem_base + i * PGSIZE;
	page_free(pg);
    }

    page_stats.pages_used = 0;
}

int
page_alloc(void **vp)
{
    struct Page_link *pl = TAILQ_FIRST(&page_free_list);
    if (pl) {
        TAILQ_REMOVE(&page_free_list, pl, pp_link);
        *vp = pl;
        page_stats.pages_avail--;
        page_stats.pages_used++;
        page_stats.allocations++;

        if (scrub_free_pages)
            memset(pl, 0xcd, PGSIZE);

        return 0;
    }

    cprintf("page_alloc: returning no mem\n");
    page_stats.failures++;
    return -E_NO_MEM;
}

void
page_free(void *v)
{
    struct Page_link *pl = (struct Page_link *) v;
    if (PGOFF(pl))
        panic("page_free: not a page-aligned pointer %p", pl);

    if (scrub_free_pages)
        memset(v, 0xde, PGSIZE);

    TAILQ_INSERT_TAIL(&page_free_list, pl, pp_link);
    page_stats.pages_avail++;
    page_stats.pages_used--;
}

void *
pa2kva(physaddr_t pa)
{
    assert(pa < (global_npages << PGSHIFT));
    return physmem_base + pa;
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
    assert(pn < global_npages);
    return pn << PGSHIFT;
}
