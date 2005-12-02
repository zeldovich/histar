#include <machine/pmap.h>
#include <kern/label.h>
#include <kern/container.h>
#include <kern/segment.h>
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
    segment_set_npages(sg, 0);
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
segment_set_npages(struct Segment *sg, uint64_t num_pages)
{
    if (num_pages > NUM_SG_PAGES)
	return -E_NO_MEM;

    for (int i = num_pages; i < sg->sg_hdr.num_pages; i++)
	page_decref(sg->sg_page[i]);

    for (int i = sg->sg_hdr.num_pages; i < num_pages; i++) {
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

static int
segment_map(struct Pagemap *pgmap, struct Segment *sg, void *va,
	    uint64_t start_page, uint64_t num_pages, bool_t writable)
{
    char *cva = (char *) va;
    if (PGOFF(cva))
	return -E_INVAL;

    if (start_page + num_pages > sg->sg_hdr.num_pages)
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
segment_map_to_pmap(struct segment_map *segmap, struct Pagemap *pgmap)
{
    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap->sm_ent[i].num_pages == 0)
	    continue;

	struct Segment *sg;
	int r = cobj_get(segmap->sm_ent[i].segment, cobj_segment, &sg);
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
