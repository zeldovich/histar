#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/pageinfo.h>
#include <kern/timer.h>
#include <inc/error.h>
#include <inc/queue.h>

enum { scrub_free_pages = 0 };
enum { page_nomem_debug = 0 };
enum { page_memstat_debug = 0 };

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

    struct page_info *pi = page_to_pageinfo(v);
    assert(!pi->pi_freepage);
    pi->pi_freepage = 1;

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

    if (!pl) {
	if (page_nomem_debug)
	    cprintf("page_alloc: out of memory: used %"PRIu64" avail %"PRIu64
		    " alloc %"PRIu64" fail %"PRIu64"\n",
		    page_stats.pages_used, page_stats.pages_avail,
		    page_stats.allocations, page_stats.failures);

	page_stats.failures++;
	pstate_sync();
	return -E_RESTART;
    }

    TAILQ_REMOVE(&page_free_list, pl, pp_link);
    *vp = pl;
    page_stats.pages_avail--;
    page_stats.pages_used++;
    page_stats.allocations++;

    if (scrub_free_pages)
	memset(pl, 0xcd, PGSIZE);

    struct page_info *pi = page_to_pageinfo(pl);
    assert(pi->pi_freepage);
    pi->pi_freepage = 0;

    return 0;
}

static void
print_memstat(void)
{
    cprintf("pagealloc: used %"PRIu64" avail %"PRIu64
	    " alloc %"PRIu64" fail %"PRIu64"\n",
	    page_stats.pages_used, page_stats.pages_avail,
	    page_stats.allocations, page_stats.failures);
}

void
page_alloc_init(void)
{
    TAILQ_INIT(&page_free_list);

    static struct periodic_task pt =
	{ .pt_fn = &print_memstat, .pt_interval_sec = 5 };
    if (page_memstat_debug)
	timer_add_periodic(&pt);
}
