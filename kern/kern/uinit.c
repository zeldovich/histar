#include <kern/console.h>
#include <kern/thread.h>
#include <kern/as.h>
#include <kern/label.h>
#include <kern/uinit.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/lib.h>
#include <kern/id.h>
#include <kern/pstate.h>
#include <kern/embedbin.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <inc/elf32.h>
#include <inc/elf64.h>
#include <inc/error.h>

#if JOS_ARCH_BITS==32
#define ARCH_ELF_CLASS	ELF_CLASS_32
#define ARCH_ELF_EHDR	Elf32_Ehdr
#define ARCH_ELF_PHDR	Elf32_Phdr
#elif JOS_ARCH_BITS==64
#define ARCH_ELF_CLASS	ELF_CLASS_64
#define ARCH_ELF_EHDR	Elf64_Ehdr
#define ARCH_ELF_PHDR	Elf64_Phdr
#else
#error Unknown arch
#endif

#if JOS_ARCH_ENDIAN==JOS_LITTLE_ENDIAN
#define ARCH_ELF_MAGIC	ELF_MAGIC_LE
#elif JOS_ARCH_ENDIAN==JOS_BIG_ENDIAN
#define ARCH_ELF_MAGIC	ELF_MAGIC_BE
#else
#error Unknown arch
#endif

uint64_t user_root_ct;

#define assert_check(expr)			\
    do {					\
	int __r = (expr);			\
	if (__r < 0)				\
	    panic("%s: %s", #expr, e2s(__r));	\
    } while (0)

struct embed_bin embed_bins[] __attribute__ ((weak)) = { { 0, 0, 0 } };

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

    assert_check(kobject_set_nbytes(&as->as_ko, PGSIZE));

    struct u_segment_mapping *usm;
    assert_check(kobject_get_page(&as->as_ko, 0, (void **)&usm, page_excl_dirty));

    usm[*smi].segment = seg;
    usm[*smi].start_page = start_page;
    usm[*smi].num_pages = num_pages;
    usm[*smi].flags = flags;
    usm[*smi].kslot = *smi;
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
	cprintf("segment_create_embed: bufsize %"PRIu64" > segsize %"PRIu64"\n",
		bufsize, segsize);
	return -E_INVAL;
    }

    struct Segment *sg;
    int r = segment_alloc(l, &sg);
    if (r < 0) {
	cprintf("segment_create_embed: cannot alloc segment: %s\n", e2s(r));
	return r;
    }

    r = segment_set_nbytes(sg, segsize);
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
	    assert_check(kobject_get_page(&sg->sg_ko, i/PGSIZE, &p, page_excl_dirty));
	    memcpy(p, &buf[i], bytes);
	}
	bufsize -= bytes;
    }

    if (sg_store)
	*sg_store = sg;
    return container_put(c, &sg->sg_ko, 0);
}

static int
thread_load_elf(struct Container *c, struct Thread *t,
		struct Label *label, struct Label *priv,
		const uint8_t *binary, uint64_t size,
		uint64_t arg0, uint64_t arg1)
{
    ARCH_ELF_EHDR elf;
    if (elf_copyin(&elf, 0, sizeof(elf), binary, size) < 0) {
	cprintf("ELF header unreadable\n");
	return -E_INVAL;
    }

    if (elf.e_magic != ARCH_ELF_MAGIC ||
	elf.e_ident[EI_CLASS] != ARCH_ELF_CLASS)
    {
	cprintf("ELF magic/class mismatch\n");
	return -E_INVAL;
    }

    uint32_t segmap_i = 0;
    struct Address_space *as;
    int r = as_alloc(label, &as);
    if (r < 0) {
	cprintf("thread_load_elf: cannot allocate AS: %s\n", e2s(r));
	return r;
    }

    r = container_put(c, &as->as_ko, 0);
    if (r < 0) {
	cprintf("thread_load_elf: cannot put AS in container: %s\n", e2s(r));
	return r;
    }

    for (int i = 0; i < elf.e_phnum; i++) {
	ARCH_ELF_PHDR ph;
	if (elf_copyin(&ph, elf.e_phoff + i * sizeof(ph), sizeof(ph),
		       binary, size) < 0)
	{
	    cprintf("ELF section header unreadable\n");
	    return -E_INVAL;
	}

	if (ph.p_type != ELF_PROG_LOAD)
	    continue;

	if (ph.p_offset + ph.p_filesz > size) {
	    cprintf("ELF: section past the end of the file\n");
	    return -E_INVAL;
	}

	char *va = (char *) (uintptr_t) ROUNDDOWN(ph.p_vaddr, PGSIZE);
	uint64_t page_offset = PGOFF(ph.p_offset);
	uint64_t mem_pages = ROUNDUP(page_offset + ph.p_memsz, PGSIZE) / PGSIZE;

	struct Segment *s;
	r = segment_create_embed(c, label,
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
    int stackpages = 4;
    struct Segment *s;
    r = segment_create_embed(c, label, stackpages * PGSIZE, 0, 0, &s);
    if (r < 0) {
	cprintf("ELF: cannot allocate stack segment: %s\n", e2s(r));
	return r;
    }

    r = elf_add_segmap(as, &segmap_i,
		       COBJ(c->ct_ko.ko_id, s->sg_ko.ko_id),
		       0, stackpages,
		       (void *) (uintptr_t) (USTACKTOP - stackpages * PGSIZE),
		       SEGMAP_READ | SEGMAP_WRITE);
    if (r < 0) {
	cprintf("ELF: cannot map stack segment: %s\n", e2s(r));
	return r;
    }

    struct Label *empty_label;
    assert_check(label_alloc(&empty_label, label_track));

    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_as = COBJ(c->ct_ko.ko_id, as->as_ko.ko_id);
    te.te_entry = (void *) (uintptr_t) elf.e_entry;
    te.te_stack = (void *) USTACKTOP;
    te.te_arg[0] = 1;
    te.te_arg[1] = arg0;
    te.te_arg[2] = arg1;
    assert_check(thread_jump(t, empty_label, priv, priv, &te));
    return 0;
}

static void
thread_create_embed(struct Container *c,
		    struct Label *label,
		    struct Label *priv,
		    const char *name,
		    uint64_t arg0, uint64_t arg1)
{
    struct embed_bin *prog = 0;

    for (int i = 0; embed_bins[i].name; i++)
	if (!strcmp(name, embed_bins[i].name))
	    prog = &embed_bins[i];

    if (prog == 0)
	panic("thread_create_embed: cannot find binary for %s", name);

    struct Container *tc;
    assert_check(container_alloc(label, &tc));
    tc->ct_ko.ko_quota_total = (((uint64_t) 1) << 32);
    strncpy(&tc->ct_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    assert(container_put(c, &tc->ct_ko, 0) >= 0);

    struct Label *empty_label;
    assert_check(label_alloc(&empty_label, label_track));

    struct Thread *t;
    assert_check(thread_alloc(empty_label, priv, priv, &t));
    strncpy(&t->th_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    thread_set_sched_parents(t, tc->ct_ko.ko_id, 0);

    assert_check(container_put(tc, &t->th_ko, 0));
    assert_check(thread_load_elf(tc, t, label, priv,
				 prog->buf, prog->size, arg0, arg1));

    thread_set_runnable(t);
}

static void
fs_init(struct Container *c, struct Label *l)
{
    for (int i = 0; embed_bins[i].name; i++) {
	const char *name = embed_bins[i].name;
	const uint8_t *buf = embed_bins[i].buf;
	uint64_t size = embed_bins[i].size;

	struct Segment *s;
	assert_check(segment_create_embed(c, l, size, buf, size, &s));
	strncpy(&s->sg_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    }
}

static void
user_bootstrap(void)
{
    key_generate();

    // root integrity categories and label
    uint64_t user_root_cat_i = id_alloc();

    struct Label *obj_label;
    assert_check(label_alloc(&obj_label, label_track));
    assert_check(label_add(obj_label, user_root_cat_i));

    // root-parent container; not readable by anyone
    uint64_t root_parent_cat_s = id_alloc() | LB_SECRECY_FLAG;
    struct Label *root_parent_label;
    assert_check(label_alloc(&root_parent_label, label_track));
    assert_check(label_add(root_parent_label, root_parent_cat_s));

    struct Container *root_parent;
    assert_check(container_alloc(root_parent_label, &root_parent));
    root_parent->ct_ko.ko_quota_total = CT_QUOTA_INF;
    kobject_incref_resv(&root_parent->ct_ko, 0);
    strncpy(&root_parent->ct_ko.ko_name[0], "root parent", KOBJ_NAME_LEN - 1);

    // root container
    struct Container *rc;
    assert_check(container_alloc(obj_label, &rc));
    rc->ct_ko.ko_quota_total = CT_QUOTA_INF;
    assert_check(container_put(root_parent, &rc->ct_ko, 0));
    strncpy(&rc->ct_ko.ko_name[0], "root container", KOBJ_NAME_LEN - 1);
    user_root_ct = rc->ct_ko.ko_id;

    // filesystem
    struct Container *fsc;
    assert_check(container_alloc(obj_label, &fsc));
    fsc->ct_ko.ko_quota_total = (((uint64_t) 1) << 32);
    fsc->ct_avoid_types = (1 << kobj_address_space) | (1 << kobj_device) |
			  (1 << kobj_gate) | (1 << kobj_thread);
    assert_check(container_put(rc, &fsc->ct_ko, 0));
    strncpy(&fsc->ct_ko.ko_name[0], "embed_bins", KOBJ_NAME_LEN - 1);

    fs_init(fsc, obj_label);

    // init process
    struct Label *init_label;
    assert_check(label_alloc(&init_label, label_track));
    assert_check(label_add(init_label, user_root_cat_i));

    struct Label *init_priv;
    assert_check(label_alloc(&init_priv, label_priv));
    assert_check(label_add(init_priv, user_root_cat_i));

    thread_create_embed(rc, init_label, init_priv, "init",
			rc->ct_ko.ko_id, user_root_cat_i);
}

static void
free_embed(void)
{
    for (int i = 0; embed_bins[i].name; i++) {
	for (int j = 0; j < i; j++) {
	    if (!strcmp(embed_bins[i].name, embed_bins[j].name)) {
		cprintf("Duplicate embedded binary: %s\n", embed_bins[i].name);
		goto skip;
	    }
	}

	void *buf_start = (void *) embed_bins[i].buf;
	void *buf_end = buf_start + embed_bins[i].size;

	for (void *b = ROUNDUP(buf_start, PGSIZE);
	     b + PGSIZE <= buf_end; b += PGSIZE)
	    page_free(pa2kva(kva2pa(b)));

 skip:
	;
    }
}

void
user_init(void)
{
    if (!pstate_part) {
	cprintf("No pstate partition found.\n");
	goto discard;
    }

    if (embed_bins[0].name) {
	if (strstr(&boot_cmdline[0], "pstate=discard")) {
	    cprintf("Command-line option pstate=discard..\n");
	    goto discard;
	}

	cprintf("Loading persistent state: hit 'x' to discard, 'z' to load.\n");
	for (int i = 0; i < 1000; i++) {
	    int c = cons_getc();
	    if (c == 'x') {
		goto discard;
	    } else if (c == 'z') {
		break;
	    }

	    timer_delay(1000000);
	}
    }

    int r = pstate_load();
    if (r < 0) {
	cprintf("Unable to load persistent state: %s\n", e2s(r));
	goto discard;
    } else {
	cprintf("Persistent state loaded OK\n");
    }

    free_embed();
    return;

 discard:
    cprintf("Discarding persistent state.\n");
    user_bootstrap();
    free_embed();
    return;
}
