#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/pstate.h>
#include <kern/pageinfo.h>
#include <kern/timer.h>
#include <inc/error.h>
#include <inc/queue.h>

enum { scrub_free_pages = 0 };
enum { page_nomem_debug = 1 };
enum { page_memstat_debug = 0 };

uint64_t global_npages;		// Amount of physical memory (in pages)

struct Page_link {
    TAILQ_ENTRY(Page_link) pp_link;	// free list link
};
static TAILQ_HEAD(Page_list, Page_link) page_free_list;
					// Free list of physical pages

#if JOS_ARCH_PAGE_BITMAP == 1
extern uint32_t *page_bitmap;		// Free page page_bitmap ($ARCH/page.c)
extern uint32_t  page_bitmap_words;	// Bitmap size in words

// Search our page_bitmap for contiguous pages. Returns the _physical_ page
// number, or -1 on failure.
//
// This isn't exactly optimal, but it can scan 16GB worth of 4K pages in
// about 500us on a Core2 2.4GHz.
static uint32_t
page_bitmap_search_fast(const uint32_t mask, const uint32_t align)
{
	unsigned int i, j, found = 0;
	uint32_t alignmask = (align == 0) ? 0 : align - 1;

	if (page_bitmap == NULL || page_bitmap_words == 0)
		return (-1);

	for (i = 0; i < page_bitmap_words; i++) {
		uint32_t word0, word1, map;

		word0 = page_bitmap[i];
		word1 = (i == (page_bitmap_words - 1)) ? 0 : page_bitmap[i + 1];

		if (word0 == 0 && word1 == 0)
			continue;

		for (j = 0; j < 32; j++) {
			map = word0;

			// avoid undefined shift by 32
			if (j != 0)
				map |= (word1 << (32 - j));

			if ((map & mask) == mask &&
			   (align == 0 || ((i * 32 + j) & alignmask) == 0)) {
				found = 1;
				break;
			}

			word0 >>= 1;
		}
		if (found)
			break;
	}

	if (i == page_bitmap_words)
		return (-1);

	return (i * 32 + j);
}

static inline int
is_page_free(unsigned int pgno)
{
	unsigned int word, bit;

	word = pgno >> 5;
	bit  = pgno & 31;

	if (word >= page_bitmap_words)
		return (0);

	return (!!(page_bitmap[word] & (1U << bit)));
}

// Search our page_bitmap for contiguous pages. Returns the _physical_ page
// number, or -1 on failure.
//
// This is a (probably) considerably slower method used for really large,
// atypical allocations (> 128KB for 4KB pages) needed by framebuffers, etc.
static uint32_t
page_bitmap_search_slow(const uint32_t count, const uint32_t align)
{
	unsigned int i, incr;

	if (page_bitmap == NULL || page_bitmap_words == 0)
		return (-1);

	incr = (align == 0) ? 1 : (align / PGSIZE);
	for (i = 0; i < global_npages; i += incr) {
		if (is_page_free(i)) {
			unsigned int j;
			for (j = 1; j < count; j++) {
				if (!is_page_free(i + j))
					break;
			}
			if (j == count)
				return (i);
		}
	}

	return (-1);
}

static void
page_bitmap_mark_free(uint32_t pgn)
{
	if (page_bitmap == NULL)
		return;
	if ((pgn >> 5) >= page_bitmap_words)
		panic("page_bitmap_mark_free: bad pgno %u", pgn);
	page_bitmap[pgn >> 5] |= (1U << (pgn & 31));
}

static void
page_bitmap_mark_alloced(uint32_t pgn)
{
	if (page_bitmap == NULL)
		return;
	if ((pgn >> 5) >= page_bitmap_words)
		panic("page_bitmap_mark_alloced: bad pgno %u", pgn);
	page_bitmap[pgn >> 5] &= ~(1U << (pgn & 31));
}

#else

static uint32_t
page_bitmap_search_fast(const uint32_t mask, const uint32_t align)
{
	return (-1);
}

static uint32_t
page_bitmap_search_slow(const uint32_t count, const uint32_t align)
{
	return (-1);
}

static void
page_bitmap_mark_free(uint32_t pgn)
{
}

static void
page_bitmap_mark_alloced(uint32_t pgn)
{
}

#endif /* JOS_ARCH_PAGE_BITMAP == 1 */

// Global page allocation stats
struct page_stats page_stats = { 0, 0, 0, 0 };

void
page_free(void *v)
{
    struct Page_link *pl = (struct Page_link *) v;
    if (PGOFF(pl))
	panic("page_free: not a page-aligned pointer %p", pl);

    struct page_info *pi = page_to_pageinfo(v);
    assert(!pi->pi_freepage);
    pi->pi_freepage = 1;
    page_bitmap_mark_free((uintptr_t)kva2pa(v) >> PGSHIFT);

    if (scrub_free_pages)
	memset(v, 0xde, PGSIZE);

    TAILQ_INSERT_HEAD(&page_free_list, pl, pp_link);
    page_stats.pages_avail++;
    page_stats.pages_used--;
}

void
page_free_n(void *v, unsigned int n)
{
    unsigned int i;

#if JOS_ARCH_PAGE_BITMAP == 1
    if (page_bitmap == NULL)
	panic("page_free_n: page_bitmap == NULL");
#endif

    assert(n > 0);

    for (i = 0; i < n; i++)
	page_free((char *)v + (i * PGSIZE));
}

int
page_alloc_n(void **vp, unsigned int n, unsigned int align)
{
    uint32_t pgno = -1;
    unsigned int i;

    if (n == 0)
	panic("page_alloc_n: n == 0");

    if (n == 1) {
        if (TAILQ_FIRST(&page_free_list) != NULL) {
	    assert(align == 0);
	    pgno = kva2pa(TAILQ_FIRST(&page_free_list)) >> PGSHIFT;
        }
    } else if (n <= 32) {
	// multipage fast-path
	uint32_t mask;

	mask = 0;
	for (i = 0; i < n; i++) {
	    mask |= (1U << i);
	}

	assert(IS_POWER_OF_2(align));
	assert((align & PGMASK) == 0);
	pgno = page_bitmap_search_fast(mask, align >> PGSHIFT);
    } else {
        // multipage slow-path
	assert(IS_POWER_OF_2(align));
	assert((align & PGMASK) == 0);
        pgno = page_bitmap_search_slow(n, align);
    }

    if (pgno == (uint32_t)-1) {
	if (page_nomem_debug)
	    cprintf("page_alloc: out of memory: used %"PRIu64" avail %"PRIu64
		    " alloc %"PRIu64" fail %"PRIu64"\n",
		    page_stats.pages_used, page_stats.pages_avail,
		    page_stats.allocations, page_stats.failures);

	page_stats.failures++;
	pstate_sync(1);
	return -E_RESTART;
    }

    // pgno points to page number of the first of 'n' contiguous physical pages
    // XXX- uint32_t pgno probably not good here for 64-bit!!!
    for (i = 0; i < n; i++) {
	struct Page_link *pl = pa2kva((pgno + i) << PGSHIFT);
	TAILQ_REMOVE(&page_free_list, pl, pp_link);

	if (i == 0)
	    *vp = pl;

	if (scrub_free_pages)
	    memset(pl, 0xcd, PGSIZE);

	struct page_info *pi = page_to_pageinfo(pl);
	assert(pi->pi_freepage);
	pi->pi_freepage = 0;
	page_bitmap_mark_alloced((uintptr_t)kva2pa((void *)pl) >> PGSHIFT);
    }

    page_stats.pages_avail -= n;
    page_stats.pages_used  += n;
    page_stats.allocations++;

    return 0;
}

int
page_alloc(void **vp)
{
    return (page_alloc_n(vp, 1, 0));
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
	{ .pt_fn = &print_memstat, .pt_interval_msec = 5 * 1000 };
    if (page_memstat_debug)
	timer_add_periodic(&pt);
}
