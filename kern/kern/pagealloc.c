#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
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
    if (!TAILQ_FIRST(&page_free_list)) {
	cprintf("page_alloc: out of memory, swapping out\n");
	cprintf("page_alloc: used %"PRIu64" avail %"PRIu64
		" alloc %"PRIu64" fail %"PRIu64"\n",
		page_stats.pages_used, page_stats.pages_avail,
		page_stats.allocations, page_stats.failures);

	pstate_sync();
	return -E_RESTART;
    }

    struct Page_link *pl = TAILQ_FIRST(&page_free_list);
    TAILQ_REMOVE(&page_free_list, pl, pp_link);
    *vp = pl;
    page_stats.pages_avail--;
    page_stats.pages_used++;
    page_stats.allocations++;

    if (scrub_free_pages)
	memset(pl, 0xcd, PGSIZE);

    return 0;
}

void
page_alloc_init(void)
{
    TAILQ_INIT(&page_free_list);
}
