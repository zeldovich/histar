#include <kern/lib.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/queue.h>

enum { scrub_free_pages = 0 };

uint64_t global_npages;		// Amount of physical memory (in pages)

struct Page_link {
    TAILQ_ENTRY(Page_link) pp_link;	// free list link
};
static TAILQ_HEAD(Page_list, Page_link) page_free_list;
					// Free list of physical pages

// Global page allocation stats
struct page_stats page_stats;

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
page_alloc_init(void)
{
    TAILQ_INIT(&page_free_list);
}
