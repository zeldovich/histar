#include <kern/segment.h>
#include <machine/pmap.h>
#include <kern/label.h>
#include <inc/error.h>

int
segment_alloc(struct Segment **sgp)
{
    struct Page *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;

    struct Segment *sg = page2kva(p);
    sg->sg_hdr.num_pages = 0;
    sg->sg_hdr.label = 0;

    *sgp = sg;
    return 0;
}

void
segment_free(struct Segment *sg)
{
    segment_set_length(sg, 0);
    if (sg->sg_hdr.label)
	label_free(sg->sg_hdr.label);

    page_free(pa2page(kva2pa(sg)));
}

void
segment_addref(struct Segment *sg)
{
    pa2page(kva2pa(sg))->pp_ref++;
}

void
segment_decref(struct Segment *sg)
{
    if (--pa2page(kva2pa(sg))->pp_ref == 0)
	segment_free(sg);
}

int
segment_set_length(struct Segment *sg, uint64_t num_pages)
{
    if (num_pages > NUM_SG_PAGES)
	return -E_NO_MEM;

    int i;
    for (i = num_pages; i < sg->sg_hdr.num_pages; i++)
	page_decref(sg->sg_page[i]);

    for (i = sg->sg_hdr.num_pages; i < num_pages; i++) {
	int r = page_alloc(&sg->sg_page[i]);
	if (r < 0) {
	    // free all the pages we allocated up to now
	    for (i--; i >= sg->sg_hdr.num_pages; i--)
		page_decref(sg->sg_page[i]);
	    return r;
	}

	sg->sg_page[i]->pp_ref++;
	memset(page2kva(sg->sg_page[i]), 0, PGSIZE);
    }

    sg->sg_hdr.num_pages = num_pages;
    return 0;
}
