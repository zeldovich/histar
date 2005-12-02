#include <machine/types.h>
#include <machine/thread.h>
#include <kern/label.h>
#include <kern/uinit.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/lib.h>
#include <kern/unique.h>
#include <inc/elf64.h>
#include <inc/error.h>

struct embedded_blob {
    uint8_t *buf;
    uint64_t size;
    const char *name;
    struct embedded_blob *next;
};
static struct embedded_blob *all_embed;

#define EMBED_DECLARE(n)					\
    struct embedded_blob embed_##n;				\
    do {							\
	extern uint8_t _binary_obj_user_##n##_start[],		\
		       _binary_obj_user_##n##_size[];		\
	embed_##n.buf = _binary_obj_user_##n##_start;		\
	embed_##n.size = (uint64_t) _binary_obj_user_##n##_size;\
	embed_##n.name = #n;					\
	embed_##n.next = all_embed;				\
	all_embed = &embed_##n;					\
    } while (0)

static int
elf_copyin(void *p, uint64_t offset, uint32_t count, uint8_t *binary, uint64_t size)
{
    if (offset + count > size) {
	cprintf("Reading past the end of ELF binary\n");
	return -E_INVAL;
    }

    memcpy(p, binary + offset, count);
    return 0;
}

static int
elf_add_segmap(struct segment_map *sm, int *smi, struct cobj_ref seg,
	       uint64_t start_page, uint64_t num_pages, void *va)
{
    if (*smi >= NUM_SG_MAPPINGS) {
	cprintf("ELF: too many segments\n");
	return -E_NO_MEM;
    }

    sm->sm_ent[*smi].segment = seg;
    sm->sm_ent[*smi].start_page = start_page;
    sm->sm_ent[*smi].num_pages = num_pages;
    sm->sm_ent[*smi].writable = 1;
    sm->sm_ent[*smi].va = va;
    (*smi)++;
    return 0;
}

static int
segment_create_embed(struct Container *c, struct Label *l, uint64_t segsize, uint8_t *buf, uint64_t bufsize)
{
    if (bufsize > segsize)
	return -E_INVAL;

    struct Segment *sg;
    int r = segment_alloc(&sg);
    if (r < 0)
	return r;

    r = label_copy(l, &sg->sg_hdr.label);
    if (r < 0) {
	segment_free(sg);
	return r;
    }

    r = segment_set_npages(sg, (segsize + PGSIZE - 1) / PGSIZE);
    if (r < 0) {
	segment_free(sg);
	return r;
    }

    for (int i = 0; bufsize > 0; i += PGSIZE) {
	uint64_t bytes = PGSIZE;
	if (bytes > bufsize)
	    bytes = bufsize;

	if (buf)
	    memcpy(page2kva(sg->sg_page[i / PGSIZE]), &buf[i], bytes);
	bufsize -= bytes;
    }

    int slot = container_put(c, cobj_segment, sg);
    if (slot < 0)
	segment_free(sg);
    return slot;
}

static int
thread_load_elf(struct Container *c, struct Thread *t, struct Label *l,
		uint8_t *binary, uint64_t size)
{
    Elf64_Ehdr elf;
    if (elf_copyin(&elf, 0, sizeof(elf), binary, size) < 0) {
	cprintf("ELF header unreadable\n");
	return -E_INVAL;
    }

    if (elf.e_magic != ELF_MAGIC || elf.e_ident[0] != 2) {
	cprintf("ELF magic mismatch\n");
	return -E_INVAL;
    }

    struct segment_map segmap;
    memset(&segmap, 0, sizeof(segmap));
    int segmap_i = 0;

    for (int i = 0; i < elf.e_phnum; i++) {
	Elf64_Phdr ph;
	if (elf_copyin(&ph, elf.e_phoff + i * sizeof(ph), sizeof(ph), binary, size) < 0) {
	    cprintf("ELF section header unreadable\n");
	    return -E_INVAL;
	}

	if (ph.p_type != 1)
	    continue;

	if (ph.p_offset + ph.p_filesz > size) {
	    cprintf("ELF: section past the end of the file\n");
	    return -E_INVAL;
	}

	char *va = (char *) ROUNDDOWN(ph.p_vaddr, PGSIZE);
	uint64_t page_offset = PGOFF(ph.p_offset);
	uint64_t mem_pages = ROUNDUP(page_offset + ph.p_memsz, PGSIZE) / PGSIZE;

	int segslot = segment_create_embed(c, l,
					   mem_pages * PGSIZE,
					   binary + ph.p_offset - page_offset,
					   page_offset + ph.p_filesz);
	if (segslot < 0) {
	    cprintf("ELF: cannot create segment\n");
	    return segslot;
	}

	int r = elf_add_segmap(&segmap, &segmap_i, COBJ(c->ct_hdr.idx, segslot),
			       0, mem_pages, va);
	if (r < 0) {
	    cprintf("ELF: cannot map segment\n");
	    return r;
	}
    }

    // Map a stack
    int stackslot = segment_create_embed(c, l, PGSIZE, 0, 0);
    if (stackslot < 0) {
	cprintf("ELF: cannot allocate stack segment\n");
	return stackslot;
    }

    int r = elf_add_segmap(&segmap, &segmap_i, COBJ(c->ct_hdr.idx, stackslot),
			   0, 1, (void*) (ULIM - PGSIZE));
    if (r < 0) {
	cprintf("ELF: cannot map stack segment\n");
	return r;
    }

    struct Label *nl;
    r = label_copy(l, &nl);
    if (r < 0)
	return r;

    thread_jump(t, nl, &segmap, (void*) elf.e_entry, (void*) ULIM, 0);
    return 0;
}

static void
thread_create_embed(struct Container *c, struct Label *l, struct embedded_blob *prog)
{
    struct Container *tc;
    int r = container_alloc(&tc);
    if (r < 0)
	panic("tce: cannot alloc container");

    r = label_copy(l, &tc->ct_hdr.label);
    if (r < 0)
	panic("tce: cannot label container");

    int tcslot = container_put(c, cobj_container, tc);
    if (tcslot < 0)
	panic("tce: cannot store container");

    struct Thread *t;
    r = thread_alloc(&t);
    if (r < 0)
	panic("tce: cannot allocate thread");

    r = container_put(tc, cobj_thread, t);
    if (r < 0)
	panic("tce: cannot store thread");

    r = thread_load_elf(tc, t, l, prog->buf, prog->size);
    if (r < 0)
	panic("tce: cannot load ELF");

    thread_set_runnable(t);
}

static void
fs_init(struct Container *c, struct Label *l)
{
    // XXX yet another constant
    assert(0 == segment_create_embed(c, l, PGSIZE, 0, 0));

    struct Segment *fs_names;
    assert(0 == cobj_get(COBJ(1, 0), cobj_segment, &fs_names));
    char *fs_dir = (char*) page2kva(fs_names->sg_page[0]);

    for (struct embedded_blob *e = all_embed; e; e = e->next) {
	int slot = segment_create_embed(c, l, e->size, e->buf, e->size);
	if (slot < 0)
	    panic("fs_init: cannot store embedded segment");

	char nent = ++fs_dir[0];
	fs_dir[nent*64] = slot;
	memcpy(&fs_dir[nent*64+1], e->name, strlen(e->name) + 1);
    }
}

void
user_init(void)
{
    EMBED_DECLARE(idle);
    EMBED_DECLARE(spin);
    EMBED_DECLARE(hello);
    EMBED_DECLARE(jmptest);
/*
    EMBED_DECLARE(gate_test);
    EMBED_DECLARE(thread_test);
*/
    EMBED_DECLARE(shell);

    // XXX have to alloc the container first, so that it gets ID 0
    struct Container *rc;
    assert(0 == container_alloc(&rc));

    // XXX alloc a "root fs" container as ID 1 for now
    struct Container *fsc;
    assert(0 == container_alloc(&fsc));
    assert(0 == container_put(rc, cobj_container, fsc));

    uint64_t root_handle = unique_alloc();
    struct Label *l;
    assert(0 == label_alloc(&l));

    l->lb_hdr.def_level = 1;
    assert(0 == label_set(l, root_handle, LB_LEVEL_STAR));
    assert(0 == label_copy(l, &rc->ct_hdr.label));
    assert(0 == label_copy(l, &fsc->ct_hdr.label));

    fs_init(fsc, l);

    thread_create_embed(rc, l, &embed_idle);
    thread_create_embed(rc, l, &embed_shell);

    label_free(l);
}
