#include <machine/pmap.h>
#include <kern/label.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <inc/error.h>

int
segment_alloc(struct Label *l, struct Segment **sgp)
{
    struct Segment *sg;
    int r = kobject_alloc(kobj_segment, l, (struct kobject **)&sg);
    if (r < 0)
	return r;

    *sgp = sg;
    return 0;
}

void
segment_gc(struct Segment *sg)
{
    segment_set_npages(sg, 0);
}

void
segment_swapout(struct Segment *sg)
{
    segment_set_npages(sg, 0);
}

int
segment_set_npages(struct Segment *sg, uint64_t num_pages)
{
    if (num_pages > NUM_SG_PAGES)
	return -E_NO_MEM;

    for (int i = num_pages; i < sg->sg_ko.ko_extra_pages; i++)
	page_free(sg->sg_page[i]);

    for (int i = sg->sg_ko.ko_extra_pages; i < num_pages; i++) {
	int r = page_alloc(&sg->sg_page[i]);
	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (i--; i >= sg->sg_ko.ko_extra_pages; i--)
		page_free(sg->sg_page[i]);
	    return r;
	}

	memset(sg->sg_page[i], 0, PGSIZE);
    }

    sg->sg_ko.ko_extra_pages = num_pages;
    return 0;
}

static int
segment_map(struct Pagemap *pgmap, struct Segment *sg, void *va,
	    uint64_t start_page, uint64_t num_pages, bool_t writable)
{
    char *cva = (char *) va;
    if (PGOFF(cva))
	return -E_INVAL;

    if (start_page + num_pages > sg->sg_ko.ko_extra_pages)
	return -E_INVAL;

    for (int i = start_page; i < start_page + num_pages; i++) {
	int r = 0;

	if (((uint64_t) cva) >= ULIM)
	    r = -E_INVAL;
	if (r == 0)
	    r = page_insert(pgmap, sg->sg_page[i], cva,
			    PTE_U | (writable ? PTE_W : 0));
	if (r < 0) {
	    // unmap pages
	    for (; i >= 0; i--) {
		page_remove(pgmap, cva);
		cva -= PGSIZE;
	    }
	    return r;
	}

	cva += PGSIZE;
    }

    return 0;
}

int
segment_map_fill_pmap(struct segment_map *segmap, struct Pagemap *pgmap, void *va)
{
    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap->sm_ent[i].num_pages == 0)
	    continue;

	void *va_start = segmap->sm_ent[i].va;
	void *va_end = va_start + segmap->sm_ent[i].num_pages * PGSIZE;
	if (va < va_start || va >= va_end)
	    continue;

	struct Segment *sg;
	int r = cobj_get(segmap->sm_ent[i].segment, kobj_segment, (struct kobject **)&sg);
	if (r < 0)
	    return r;

	r = segment_map(pgmap, sg,
			segmap->sm_ent[i].va,
			segmap->sm_ent[i].start_page,
			segmap->sm_ent[i].num_pages,
			segmap->sm_ent[i].writable);
	if (r < 0)
	    return r;
    }

    return 0;
}

void
segment_swapin_page(struct Segment *sg, uint64_t page_num, void *p)
{
    sg->sg_page[page_num] = p;
}

void *
segment_swapout_page(struct Segment *sg, uint64_t page_num)
{
    return sg->sg_page[page_num];
}
