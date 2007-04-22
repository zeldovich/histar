#include <machine/x86.h>
#include <dev/console.h>
#include <kern/thread.h>
#include <kern/as.h>
#include <kern/label.h>
#include <kern/uinit.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/lib.h>
#include <kern/handle.h>
#include <kern/pstate.h>
#include <kern/embedbin.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <inc/elf32.h>
#include <inc/elf64.h>
#include <inc/error.h>

uint64_t user_root_handle;

#define assert_check(expr)			\
    do {					\
	int __r = (expr);			\
	if (__r < 0)				\
	    panic("%s: %s", #expr, e2s(__r));	\
    } while (0)

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

    sg->sg_ko.ko_flags |= c->ct_ko.ko_flags;
    if (sg_store)
	*sg_store = sg;
    return container_put(c, &sg->sg_ko);
}

#define ELF_LOAD_MACRO(bits)						\
static int								\
elf_load##bits(Elf##bits##_Ehdr *elf,					\
	       struct Address_space *as,				\
	       struct Container *c, struct Label *obj_label,		\
	       const uint8_t *binary, uint64_t size,			\
	       uint32_t *segmap_ip,					\
	       uintptr_t *entryp)					\
{									\
    for (uint32_t i = 0; i < elf->e_phnum; i++) {			\
	Elf##bits##_Phdr ph;						\
	if (elf_copyin(&ph, elf->e_phoff + i * sizeof(ph), sizeof(ph),	\
		       binary, size) < 0)				\
	{								\
	    cprintf("ELF section header unreadable\n");			\
	    return -E_INVAL;						\
	}								\
									\
	if (ph.p_type != ELF_PROG_LOAD)					\
	    continue;							\
									\
	if (ph.p_offset + ph.p_filesz > size) {				\
	    cprintf("ELF: section past the end of the file\n");		\
	    return -E_INVAL;						\
	}								\
									\
	char *va = (char *) (uintptr_t) ROUNDDOWN(ph.p_vaddr, PGSIZE);	\
	uintptr_t page_offset = PGOFF(ph.p_offset);			\
	uintptr_t mem_pages = ROUNDUP(page_offset + ph.p_memsz,		\
				      PGSIZE) / PGSIZE;			\
									\
	struct Segment *s;						\
	int r = segment_create_embed(c, obj_label,			\
				     mem_pages * PGSIZE,		\
				     binary + ph.p_offset - page_offset,\
				     page_offset + ph.p_filesz, &s);	\
	if (r < 0) {							\
	    cprintf("ELF: cannot create segment: %s\n", e2s(r));	\
	    return r;							\
	}								\
									\
	r = elf_add_segmap(as, segmap_ip,				\
			   COBJ(c->ct_ko.ko_id, s->sg_ko.ko_id),	\
			   0, mem_pages, va, ph.p_flags);		\
	if (r < 0) {							\
	    cprintf("ELF: cannot map segment\n");			\
	    return r;							\
	}								\
    }									\
									\
    *entryp = elf->e_entry;						\
    return 0;								\
}

ELF_LOAD_MACRO(32)
ELF_LOAD_MACRO(64)
#undef ELF_LOAD_MACRO

static int
thread_load_elf(struct Container *c, struct Thread *t,
		struct Label *obj_label,
		struct Label *th_label,
		struct Label *th_clearance,
		const uint8_t *binary, uint64_t size,
		uint64_t arg0, uint64_t arg1)
{
    union {
	Elf32_Ehdr elf32;
	Elf64_Ehdr elf64;
    } elf;

    if (elf_copyin(&elf, 0, sizeof(elf), binary, size) < 0) {
	cprintf("ELF header unreadable\n");
	return -E_INVAL;
    }

    if (elf.elf32.e_magic != ELF_MAGIC) {
	cprintf("ELF magic mismatch\n");
	return -E_INVAL;
    }

    uint32_t segmap_i = 0;
    struct Address_space *as;
    int r = as_alloc(obj_label, &as);
    if (r < 0) {
	cprintf("thread_load_elf: cannot allocate AS: %s\n", e2s(r));
	return r;
    }
    as->as_ko.ko_flags |= c->ct_ko.ko_flags;

    r = container_put(c, &as->as_ko);
    if (r < 0) {
	cprintf("thread_load_elf: cannot put AS in container: %s\n", e2s(r));
	return r;
    }

    uintptr_t elf_entry;
    if (elf.elf32.e_ident[EI_CLASS] == ELF_CLASS_64) {
	r = elf_load64(&elf.elf64, as, c, obj_label, binary, size,
		       &segmap_i, &elf_entry);
    } else if (elf.elf32.e_ident[EI_CLASS] == ELF_CLASS_32) {
	r = elf_load32(&elf.elf32, as, c, obj_label, binary, size,
		       &segmap_i, &elf_entry);
    } else {
	cprintf("thread_load_elf: bad class %d\n", elf.elf32.e_ident[EI_CLASS]);
	r = -E_INVAL;
    }

    if (r < 0)
	return r;

    // Map a stack
    int stackpages = 2;
    struct Segment *s;
    r = segment_create_embed(c, obj_label, stackpages * PGSIZE, 0, 0, &s);
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

    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_as = COBJ(c->ct_ko.ko_id, as->as_ko.ko_id);
    te.te_entry = (void *) (uintptr_t) elf_entry;
    te.te_stack = (void *) USTACKTOP;
    te.te_arg[0] = 1;
    te.te_arg[1] = arg0;
    te.te_arg[2] = arg1;
    assert_check(thread_jump(t, th_label, th_clearance, &te));
    return 0;
}

static void
thread_create_embed(struct Container *c,
		    struct Label *obj_label,
		    struct Label *th_label,
		    struct Label *th_clearance,
		    const char *name,
		    uint64_t arg0, uint64_t arg1,
		    uint64_t koflag)
{
    // pin the labels of the idle process
    obj_label->lb_ko.ko_flags |= koflag;
    th_label->lb_ko.ko_flags |= koflag;
    th_clearance->lb_ko.ko_flags |= koflag;

    struct embed_bin *prog = 0;

    for (int i = 0; embed_bins[i].name; i++)
	if (!strcmp(name, embed_bins[i].name))
	    prog = &embed_bins[i];

    if (prog == 0)
	panic("thread_create_embed: cannot find binary for %s", name);

    struct Container *tc;
    assert_check(container_alloc(obj_label, &tc));
    tc->ct_ko.ko_quota_total = (((uint64_t) 1) << 32);
    tc->ct_ko.ko_flags |= koflag;
    strncpy(&tc->ct_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    assert(container_put(c, &tc->ct_ko) >= 0);

    struct Thread *t;
    assert_check(thread_alloc(th_label, th_clearance, &t));
    t->th_ko.ko_flags |= tc->ct_ko.ko_flags;
    strncpy(&t->th_ko.ko_name[0], name, KOBJ_NAME_LEN - 1);
    thread_set_sched_parents(t, tc->ct_ko.ko_id, 0);

    assert_check(container_put(tc, &t->th_ko));
    assert_check(thread_load_elf(tc, t,
				 obj_label, th_label, th_clearance,
				 prog->buf, prog->size, arg0, arg1));

    /* XXX should go away when we have real CPU resource allocation */
    if ((koflag & KOBJ_PIN_IDLE))
	t->th_sched_tickets = 1;

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
    handle_key_generate();

    // root handle and a label
    user_root_handle = handle_alloc();

    struct Label *obj_label;
    assert_check(label_alloc(&obj_label, 1));
    assert_check(label_set(obj_label, user_root_handle, 0));

    // root-parent container; not readable by anyone
    struct Label *root_parent_label;
    struct Container *root_parent;
    assert_check(label_alloc(&root_parent_label, 3));
    assert_check(container_alloc(root_parent_label, &root_parent));
    root_parent->ct_ko.ko_quota_total = CT_QUOTA_INF;
    kobject_incref_resv(&root_parent->ct_ko, 0);
    strncpy(&root_parent->ct_ko.ko_name[0], "root parent", KOBJ_NAME_LEN - 1);

    // root container
    struct Container *rc;
    assert_check(container_alloc(obj_label, &rc));
    rc->ct_ko.ko_quota_total = CT_QUOTA_INF;
    assert_check(container_put(root_parent, &rc->ct_ko));
    strncpy(&rc->ct_ko.ko_name[0], "root container", KOBJ_NAME_LEN - 1);

    // filesystem
    struct Container *fsc;
    assert_check(container_alloc(obj_label, &fsc));
    fsc->ct_ko.ko_quota_total = (((uint64_t) 1) << 32);
    fsc->ct_avoid_types = (1 << kobj_address_space) | (1 << kobj_netdev) |
			  (1 << kobj_gate) | (1 << kobj_thread);
    assert_check(container_put(rc, &fsc->ct_ko));
    strncpy(&fsc->ct_ko.ko_name[0], "embed_bins", KOBJ_NAME_LEN - 1);

    fs_init(fsc, obj_label);

    // idle: thread { idle:* 1 }, objects { idle:0 1 }, clearance { idle:3 2 }
    struct Label *idle_th_label;
    struct Label *idle_obj_label;
    struct Label *idle_th_clear;
    assert_check(label_alloc(&idle_th_label, 1));
    assert_check(label_alloc(&idle_obj_label, 1));
    assert_check(label_alloc(&idle_th_clear, 2));

    uint64_t idle_handle = handle_alloc();
    assert_check(label_set(idle_th_label, idle_handle, LB_LEVEL_STAR));
    assert_check(label_set(idle_obj_label, idle_handle, 0));
    assert_check(label_set(idle_th_clear, idle_handle, 3));
    thread_create_embed(rc, idle_obj_label, idle_th_label, idle_th_clear,
			"idle", 1, 1, KOBJ_PIN_IDLE);

    // init: thread { uroot:* }, objects { uroot:0 1 }, clearance { uroot:3 2 }
    struct Label *init_th_label;
    struct Label *init_obj_label;
    struct Label *init_th_clear;
    assert_check(label_alloc(&init_th_label, 1));
    assert_check(label_alloc(&init_obj_label, 1));
    assert_check(label_alloc(&init_th_clear, 2));

    assert_check(label_set(init_th_label, user_root_handle, LB_LEVEL_STAR));
    assert_check(label_set(init_obj_label, user_root_handle, 0));
    assert_check(label_set(init_th_clear, user_root_handle, 3));

    thread_create_embed(rc, init_obj_label, init_th_label, init_th_clear,
			"init", rc->ct_ko.ko_id, user_root_handle, 0);
}

static void
free_embed(void)
{
    for (int i = 0; embed_bins[i].name; i++) {
	void *buf_start = (void *) embed_bins[i].buf;
	void *buf_end = buf_start + embed_bins[i].size;

	for (void *b = ROUNDUP(buf_start, PGSIZE);
	     b + PGSIZE <= buf_end; b += PGSIZE)
	    page_free(b);
    }
}

void
user_init(void)
{
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
