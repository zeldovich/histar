#include <machine/types.h>
#include <machine/thread.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/as.h>
#include <dev/console.h>
#include <dev/kclock.h>
#include <kern/label.h>
#include <kern/uinit.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/lib.h>
#include <kern/handle.h>
#include <kern/pstate.h>
#include <kern/embedbin.h>
#include <inc/elf64.h>
#include <inc/error.h>

uint64_t user_root_handle;

static int
elf_copyin(void *p, uint64_t offset, uint32_t count,
	   const uint8_t *binary, uint64_t size)
{
    if (offset + count > size) {
	cprintf("Reading past the end of ELF binary\n");
	return -E_INVAL;
    }

    memcpy(p, binary + offset, count);
    return 0;
}

static int
elf_add_segmap(struct Address_space *as, uint32_t *smi, struct cobj_ref seg,
	       uint64_t start_page, uint64_t num_pages, void *va, uint64_t flags)
{
    if (*smi >= N_SEGMAP_PER_PAGE) {
	cprintf("ELF: too many segments\n");
	return -E_NO_MEM;
    }

    assert(0 == kobject_set_npages(&as->as_ko, 1));

    struct u_segment_mapping *usm;
    assert(0 == kobject_get_page(&as->as_ko, 0, (void **)&usm, page_rw));

    usm[*smi].segment = seg;
    usm[*smi].start_page = start_page;
    usm[*smi].num_pages = num_pages;
    usm[*smi].flags = flags;
    usm[*smi].va = va;
    (*smi)++;
    return 0;
}

static int
segment_create_embed(struct Container *c, struct Label *l, uint64_t segsize,
		     const uint8_t *buf, uint64_t bufsize,
		     struct Segment **sg_store)
{
    if (bufsize > segsize) {
	cprintf("segment_create_embed: bufsize %ld > segsize %ld\n",
		bufsize, segsize);
	return -E_INVAL;
    }

    struct Segment *sg;
    int r = segment_alloc(l, &sg);
    if (r < 0) {
	cprintf("segment_create_embed: cannot alloc segment: %s\n", e2s(r));
	return r;
    }

    r = segment_set_npages(sg, (segsize + PGSIZE - 1) / PGSIZE);
    if (r < 0) {
	cprintf("segment_create_embed: cannot grow segment: %s\n", e2s(r));
	return r;
    }

    for (int i = 0; bufsize > 0; i += PGSIZE) {
	uint64_t bytes = PGSIZE;
	if (bytes > bufsize)
	    bytes = bufsize;

	if (buf) {
	    void *p;
	    r = kobject_get_page(&sg->sg_ko, i/PGSIZE, &p, page_rw);
	    if (r < 0)
		panic("segment_create_embed: cannot get page: %s", e2s(r));

	    memcpy(p, &buf[i], bytes);
	}
	bufsize -= bytes;
    }

    sg->sg_ko.ko_flags = c->ct_ko.ko_flags;
    if (sg_store)
	*sg_store = sg;
    return container_put(c, &sg->sg_ko);
}

static int
thread_load_elf(struct Container *c, struct Thread *t, struct Label *l,
		const uint8_t *binary, uint64_t size, uint64_t arg)
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

    int segmap_i = 0;
    struct Address_space *as;
    int r = as_alloc(l, &as);
    if (r < 0) {
	cprintf("thread_load_elf: cannot allocate AS: %s\n", e2s(r));
	return r;
    }
    as->as_ko.ko_flags = c->ct_ko.ko_flags;

    r = container_put(c, &as->as_ko);
    if (r < 0) {
	cprintf("thread_load_elf: cannot put AS in container: %s\n", e2s(r));
	return r;
    }

    for (int i = 0; i < elf.e_phnum; i++) {
	Elf64_Phdr ph;
	if (elf_copyin(&ph, elf.e_phoff + i * sizeof(ph), sizeof(ph),
		       binary, size) < 0)
	{
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

	struct Segment *s;
	r = segment_create_embed(c, l,
				 mem_pages * PGSIZE,
				 binary + ph.p_offset - page_offset,
				 page_offset + ph.p_filesz, &s);
	if (r < 0) {
	    cprintf("ELF: cannot create segment: %s\n", e2s(r));
	    return r;
	}

	r = elf_add_segmap(as, &segmap_i,
			   COBJ(c->ct_ko.ko_id, s->sg_ko.ko_id),
			   0, mem_pages, va, ph.p_flags);
	if (r < 0) {
	    cprintf("ELF: cannot map segment\n");
	    return r;
	}
    }

    // Map a stack
    struct Segment *s;
    r = segment_create_embed(c, l, PGSIZE, 0, 0, &s);
    if (r < 0) {
	cprintf("ELF: cannot allocate stack segment: %s\n", e2s(r));
	return r;
    }

    r = elf_add_segmap(as, &segmap_i,
		       COBJ(c->ct_ko.ko_id, s->sg_ko.ko_id),
		       0, 1, (void*) (USTACKTOP - PGSIZE),
		       SEGMAP_READ | SEGMAP_WRITE);
    if (r < 0) {
	cprintf("ELF: cannot map stack segment: %s\n", e2s(r));
	return r;
    }

    assert(0 == thread_jump(t, l, COBJ(c->ct_ko.ko_id, as->as_ko.ko_id),
			    (void*) elf.e_entry, (void*) USTACKTOP,
			    c->ct_ko.ko_id, arg, 0));
    return 0;
}

static void
thread_create_embed(struct Container *c, struct Label *l,
		    const char *name, uint64_t arg, uint64_t koflag)
{
    struct embed_bin *prog = 0;

    for (int i = 0; embed_bins[i].name; i++)
	if (!strcmp(name, embed_bins[i].name))
	    prog = &embed_bins[i];

    if (prog == 0)
	panic("thread_create_embed: cannot find binary for %s", name);

    struct Container *tc;
    int r = container_alloc(l, &tc);
    if (r < 0)
	panic("tce: cannot alloc container: %s", e2s(r));
    tc->ct_ko.ko_flags = koflag;
    strncpy(&tc->ct_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);

    int tcslot = container_put(c, &tc->ct_ko);
    if (tcslot < 0)
	panic("tce: cannot store container: %s", e2s(tcslot));

    struct Thread *t;
    r = thread_alloc(l, &t);
    if (r < 0)
	panic("tce: cannot allocate thread: %s", e2s(r));
    t->th_ko.ko_flags = tc->ct_ko.ko_flags;
    strncpy(&t->th_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);

    r = container_put(tc, &t->th_ko);
    if (r < 0)
	panic("tce: cannot store thread: %s", e2s(r));

    r = thread_load_elf(tc, t, l, prog->buf, prog->size, arg);
    if (r < 0)
	panic("tce: cannot load ELF: %s", e2s(r));

    thread_set_runnable(t);
}

static void
fs_init(struct Container *c, struct Label *l)
{
    // Directory block is segment #0 in fs container
    struct Segment *fs_names;
    assert(0 == segment_create_embed(c, l, 4*PGSIZE, 0, 0, &fs_names));
    strncpy(&fs_names->sg_ko.ko_name[0], "directory", KOBJ_NAME_LEN - 1);

    uint64_t *fs_dir;
    assert(0 == kobject_get_page(&fs_names->sg_ko, 0,
				 (void**)&fs_dir, page_rw));

    for (int i = 0; embed_bins[i].name; i++) {
	const char *name = embed_bins[i].name;
	const uint8_t *buf = embed_bins[i].buf;
	uint64_t size = embed_bins[i].size;

	struct Segment *s;
	int r = segment_create_embed(c, l, size, buf, size, &s);
	if (r < 0)
	    panic("fs_init: cannot store embedded segment: %s", e2s(r));

	uint64_t nent = ++fs_dir[0];
	fs_dir[nent*16] = s->sg_ko.ko_id;
	memcpy(&fs_dir[nent*16+1], name, strlen(name) + 1);
	strncpy(&s->sg_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    }
}

static void
user_bootstrap(void)
{
    // root handle and a label
    user_root_handle = handle_alloc();
    struct Label l;
    label_init(&l, 1);
    assert(0 == label_set(&l, user_root_handle, LB_LEVEL_STAR));

    // root container
    struct Container *rc;
    assert(0 == container_alloc(&l, &rc));
    kobject_incref(&rc->ct_ko);
    strncpy(&rc->ct_ko.ko_name[0], "root container", KOBJ_NAME_LEN - 1);

    // filesystem
    struct Container *fsc;
    assert(0 == container_alloc(&l, &fsc));
    assert(0 == container_put(rc, &fsc->ct_ko));
    strncpy(&fsc->ct_ko.ko_name[0], "fs root", KOBJ_NAME_LEN - 1);

    fs_init(fsc, &l);

    // idle thread + init
    thread_create_embed(rc, &l, "idle", rc->ct_ko.ko_id, KOBJ_PIN_IDLE);
    thread_create_embed(rc, &l, "init", rc->ct_ko.ko_id, 0);
}

void
user_init(void)
{
    int discard = 0;
    cprintf("Loading persistent state: hit 'x' to discard, 'z' to load.\n");
    for (int i = 0; i < 1000; i++) {
	int c = cons_getc();
	if (c == 'x') {
	    discard = 1;
	    break;
	} else if (c == 'z') {
	    break;
	}

	kclock_delay(1000);
    }

    if (discard) {
	cprintf("Discarding persistent state.\n");
	user_bootstrap();
	return;
    }

    int r = pstate_load();
    if (r < 0) {
	cprintf("Unable to load persistent state: %s\n", e2s(r));
	user_bootstrap();
    } else {
	cprintf("Persistent state loaded OK\n");
    }
}
