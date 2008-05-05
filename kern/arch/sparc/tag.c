#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <machine/trap.h>
#include <machine/psr.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/label.h>
#include <kern/handle.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <inc/error.h>
#include <machine/sparc-common.h>

extern const uint8_t stext[],   etext[];
extern const uint8_t srodata[], erodata[];
extern const uint8_t sdata[],   edata[];
extern const uint8_t srwdata[], erwdata[];
extern const uint8_t sbss[],    ebss[];

extern const uint8_t kstack[], kstack_top[];
extern const uint8_t monstack[], monstack_top[];
extern const uint8_t extra_stack[], extra_stack_top[];

static uint32_t tag_table[65536] __attribute__((aligned(4096)));

uint32_t moncall_dummy;
uint32_t cur_stack_base;

struct pcall_stack {
    uint8_t *stack_base;
    uint8_t *stack_top;
    struct Trapframe tf;
    uint32_t pcall_dtag;
    uintptr_t prev_stack_base;
};

static struct pcall_stack pcall_stack[PCALL_DEPTH];
static uint32_t pcall_next;

enum { tag_trap_debug = 1 };

#if TAG_DEBUG
static uint32_t pctag_dtag_count[1 << TAG_PC_BITS] __krw__;
#endif

const struct Label dtag_label[DTAG_DYNAMIC];
const struct Thread *cur_mon_thread;
static uint32_t cur_pcall_dtag;

const char* moncall_names[] = {
    "zero", "pcall", "preturn", "tagset", "dtag alloc", "thread switch",
    "kobj alloc", "cat alloc", "set label", "set clear",
    "gate enter", "thread start", "kobj free", "kobj gc", "label compare",
};

const char* const cause_table[] = {
    [ET_CAUSE_PCV]   = "PC tag invalid",
    [ET_CAUSE_DV]    = "Data tag invalid",
    [ET_CAUSE_READ]  = "Read",
    [ET_CAUSE_WRITE] = "Write",
    [ET_CAUSE_EXEC]  = "Execute",
};

extern uint64_t nkobjects;

static void
tag_flushperm(void)
{
    /* 32-way cache, hash is low bits */
    for (uint32_t i = 0; i < 32; i++)
	wrtperm(i, 0);
}

static const struct Label *
tag_to_label(uint32_t dtag)
{
    return (const struct Label *) ROUNDDOWN(dtag, PGSIZE);
}

uint32_t
label_to_tag(const struct Label *l)
{
    /* lower 4 bits are index for the perm cache */
    uint32_t lhash = ((uintptr_t) l) >> 12;
    while (lhash & ~0xf)
	lhash = (lhash & 0xf) ^ (lhash >> 4);

    return ((uintptr_t) l) | lhash;
}

void
tag_set(const void *addr, uint32_t dtag, size_t n)
{
    if (!(read_tsr() & TSR_T)) {
	int r = monitor_call(MONCALL_TAGSET, addr, dtag, n);
	if (r != 0)
	    cprintf("tag_set: moncall_tagset: %s (%d)\n", e2s(r), r);
	return;
    }

#if TAG_DEBUG
    uint32_t start = karch_get_tsc();
#endif

    int changed_tagtable = 0;

    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));
    assert(!(n & 3));

    uint32_t i = 0;
    while (i < n) {
	const void *xaddr = addr + i;
	uint32_t ppn = pa2ppn(kva2pa(xaddr));

	if (!PGOFF(xaddr) && n - i >= PGSIZE) {
	    tag_table[ppn] = dtag;
	    changed_tagtable = 1;
	    i += PGSIZE;
	} else {
	    if (tag_table[ppn] != DTAG_PERWORD) {
		uint32_t old_tag = tag_table[ppn];
		tag_table[ppn] = DTAG_PERWORD;
		sta_mmuflush(0x400);

		for (uint32_t j = 0; j < PGSIZE; j += 4)
		    write_dtag(pa2kva(ppn2pa(ppn)) + j, old_tag);
	    }

	    write_dtag(xaddr, dtag);
	    i += 4;
	}
    }

    if (changed_tagtable)
	sta_mmuflush(0x400);
#if TAG_DEBUG
    prof_tagstuff(1, karch_get_tsc() - start);
#endif
}

static void
tag_print_label(const char *msg, const struct Label *l)
{
    cprintf("%s %p: ", msg, l);
    label_cprint(l);
}

/*
 * Tag comparison logic
 */

static int
tag_compare(uint32_t dtag, int write)
{
    if (!cur_mon_thread) {
	wrtperm(dtag, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
	return 0;
    }

    if (cur_pcall_dtag == dtag) {
	wrtperm(dtag, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
	return 0;
    }

    if (dtag < DTAG_DYNAMIC) {
	switch (dtag) {
	case DTAG_DEVICE: case DTAG_KRW: case DTAG_TYPE_SYNC: case DTAG_STACK_0:
	    wrtperm(dtag, TAG_PERM_READ | TAG_PERM_WRITE);
	    return 0;

	case DTAG_KEXEC: case DTAG_KRO: case DTAG_TYPE_KOBJ:
	    if (!write) {
		wrtperm(dtag, TAG_PERM_READ | TAG_PERM_EXEC);
		return 0;
	    }

	default:
	    ;
	}

	if (dtag >= DTAG_STACK_EX && dtag <= DTAG_STACK_EXL) {
	    uint32_t stacknum = dtag - DTAG_STACK_EX;
	    if (cur_stack_base == (uintptr_t) pcall_stack[stacknum].stack_base) {
		wrtperm(dtag, TAG_PERM_READ | TAG_PERM_WRITE);
		return 0;
	    }
	}

	return -E_LABEL;
    }

    const struct Label *cur_label;
    assert(0 == kobject_get_label(&cur_mon_thread->th_ko,
				  kolabel_contaminate, &cur_label));

    int r = label_compare(tag_to_label(dtag), cur_label, label_leq_starhi, 1);
    if (r < 0) {
	cprintf("tag_compare: cannot read\n");
	tag_print_label("PC label", cur_label);
	tag_print_label("Data label", tag_to_label(dtag));
	return r;
    }

    if (write) {
	r = label_compare(cur_label, tag_to_label(dtag), label_leq_starlo, 1);
	if (r < 0) {
	    cprintf("tag_compare: cannot write\n");
	    tag_print_label("PC label", cur_label);
	    tag_print_label("Data label", tag_to_label(dtag));
	    return r;
	}

	wrtperm(dtag, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
    } else {
	wrtperm(dtag, TAG_PERM_READ | TAG_PERM_EXEC);
    }

    return 0;
}

/*
 * Monitor call support
 */

static int32_t
moncall_tagset(void *addr, uint32_t dtag, uint32_t nbytes)
{
    if (((uintptr_t) addr) & 3)
	return -E_INVAL;

    if (nbytes & 3)
	return -E_INVAL;

    if (dtag == DTAG_TYPE_KOBJ || dtag == DTAG_TYPE_SYNC)
	return -E_INVAL;

    uint32_t i = 0;
    uint32_t ok = 1;
    uint32_t last_dtag = ~0;
    while (i < nbytes) {
	const void *xaddr = addr + i;
	uint32_t ppn = pa2ppn(kva2pa(xaddr));

	if (tag_table[ppn] == DTAG_PERWORD) {
	    uint32_t cur_tag = read_dtag(xaddr);
	    if (last_dtag != cur_tag && tag_compare(cur_tag, 1) < 0)
		ok = 0;
	    last_dtag = cur_tag;
	    i += 4;
	} else {
	    uint32_t cur_tag = tag_table[ppn];
	    if (last_dtag != cur_tag && tag_compare(cur_tag, 1) < 0)
		ok = 0;
	    last_dtag = cur_tag;
	    i += PGSIZE;
	}
    }

    if (ok)
	tag_set(addr, dtag, i);
    else
	cprintf("tag_set: not ok\n");

    return 0;
}

static void __attribute__((noreturn))
tag_moncall(struct Trapframe *tf)
{
#if TAG_DEBUG
    uint32_t start = karch_get_tsc();
#endif
    uint32_t callnum = tf->tf_regs.l0;

    if (!(tf->tf_psr & PSR_PS)) {
	cprintf("tag_moncall: from user mode at 0x%x, 0x%x\n",
		tf->tf_pc, tf->tf_npc);
	tf->tf_pc = tf->tf_npc;
	tf->tf_npc = tf->tf_npc + 4;
	goto out;
    }

    tf->tf_pc = tf->tf_npc;
    tf->tf_npc = tf->tf_npc + 4;

    struct pcall_stack *ps;
    int64_t retval = 0;
    int put_retval = 0;

    switch (callnum) {
    case MONCALL_LABEL_COMPARE: {
	const struct Label *l1 = (const struct Label *) tf->tf_regs.i1;
	const struct Label *l2 = (const struct Label *) tf->tf_regs.i2;
	level_comparator cmp = (level_comparator) tf->tf_regs.i3;

	tag_is_kobject(l1, kobj_label);
	tag_is_kobject(l2, kobj_label);
	assert(cmp == label_leq_starlo || cmp == label_leq_starhi || cmp == label_eq);

	put_retval = 1;
	retval = label_compare(l1, l2, cmp, 1);
	break;
    }

    case MONCALL_FLUSHPERM:
	tag_flushperm();
	break;

    case MONCALL_PCALL: {
	assert(pcall_next < PCALL_DEPTH);

	uint32_t pcall_idx = pcall_next++;
	ps = &pcall_stack[pcall_idx];
	memcpy(&ps->tf, tf, sizeof(*tf));
	ps->pcall_dtag = cur_pcall_dtag;
	ps->prev_stack_base = cur_stack_base;

	uint32_t magic_dtag = tf->tf_regs.l1;

	tf->tf_pc = (uintptr_t) &pcall_trampoline;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.sp = ((uintptr_t) ps->stack_top) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	cur_stack_base = (uintptr_t) ps->stack_base;
	cur_pcall_dtag = magic_dtag;
	break;
    }

    case MONCALL_KOBJ_GC: {
	const struct kobject *ko = (const struct kobject *) tf->tf_regs.i1;

	tag_is_kobject(ko, kobj_any);
	if (ko->hdr.ko_type == kobj_label) {
	    put_retval = 1;
	    retval = kobject_gc(kobject_dirty(&ko->hdr));
	    break;
	}

	assert(pcall_next < PCALL_DEPTH);
	uint32_t pcall_idx = pcall_next++;
	ps = &pcall_stack[pcall_idx];
	memcpy(&ps->tf, tf, sizeof(*tf));
	ps->pcall_dtag = cur_pcall_dtag;
	ps->prev_stack_base = cur_stack_base;

	const struct Label *ko_l;
	assert(0 == kobject_get_label(&ko->hdr, kolabel_contaminate, &ko_l));

	tf->tf_pc = (uintptr_t) &pcall_trampoline;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.l3 = (uintptr_t) &kobject_gc;
	tf->tf_regs.i0 = (uintptr_t) ko;
	tf->tf_regs.sp = ((uintptr_t) ps->stack_top) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	cur_stack_base = (uintptr_t) ps->stack_base;
	cur_pcall_dtag = label_to_tag(ko_l);
	break;
    }

    case MONCALL_PRETURN: {
	if (pcall_next == 0)
	    panic("MONCALL_PRETURN: nothing on the pstack\n");

	uint32_t pcall_idx = --pcall_next;
	ps = &pcall_stack[pcall_idx];
	ps->tf.tf_regs.i0 = tf->tf_regs.o0;
	ps->tf.tf_regs.i1 = tf->tf_regs.o1;
	tf = &ps->tf;

	/* remove permissions for previous stack */
	wrtperm(DTAG_STACK_EX + pcall_idx, 0);

	cur_pcall_dtag = ps->pcall_dtag;
	cur_stack_base = ps->prev_stack_base;
	break;
    }

    case MONCALL_TAGSET:
	put_retval = 1;
	retval = moncall_tagset((void *) tf->tf_regs.i1,
				tf->tf_regs.i2,
				tf->tf_regs.i3);
	break;

    case MONCALL_THREAD_SWITCH: {
	const struct Thread *tptr = (const struct Thread *) tf->tf_regs.i1;
	assert(tptr);
	tag_is_kobject(tptr, kobj_thread);
	cur_thread = tptr;
	cur_mon_thread = tptr;

	tf->tf_pc = (uintptr_t) &thread_arch_run;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.sp = ((uintptr_t) &kstack_top[0]) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	tf->tf_regs.o0 = (uintptr_t) tptr;
	break;
    }

    case MONCALL_KOBJ_FREE: {
	put_retval = 1;
	retval = 0;

	struct kobject *ko = (struct kobject *) tf->tf_regs.i1;
	tag_is_kobject(ko, kobj_any);

	for (int i = 0; i < kolabel_max; i++)
	    assert(0 == kobject_set_label(&ko->hdr, i, 0));
	LIST_REMOVE(ko, ko_hash);
	page_free(ko);
	nkobjects--;
	break;
    }

    case MONCALL_KOBJ_ALLOC: {
	uint8_t type = tf->tf_regs.i1;
	const struct Label *l = (const struct Label *) tf->tf_regs.i2;
	const struct Label *clear = (const struct Label *) tf->tf_regs.i3;
	struct kobject **kp = (struct kobject **) tf->tf_regs.i4;

	if (type != kobj_label)
	    tag_is_kobject(l, kobj_label);
	if (clear)
	    tag_is_kobject(clear, kobj_label);

	uint32_t ptr_tag = read_dtag(kp);
	assert(ptr_tag >= DTAG_STACK_0 && ptr_tag <= DTAG_STACK_EXL);

	if (l && cur_mon_thread) {
	    int r;
	    r = label_compare_id(cur_mon_thread->th_ko.ko_label[kolabel_contaminate], l->lb_ko.ko_id, label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object label too low\n");
		//tag_print_label_id("Thread label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
		//tag_print_label_id("Object label", l->lb_ko.ko_id);
	    }

	    r = label_compare_id(l->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object label too high\n");
		//tag_print_label_id("Object label", l->lb_ko.ko_id);
		//tag_print_label_id("Thread clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	    }
	}

	if (clear && cur_mon_thread) {
	    int r;
	    r = label_compare_id(clear->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object clearance too high\n");
		//tag_print_label_id("Object clear", clear->lb_ko.ko_id);
		//tag_print_label_id("Thread clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	    }
	}

	put_retval = 1;
	retval = kobject_alloc_real(type, l, clear, kp);
	break;
    }

    case MONCALL_SET_LABEL: {
	const struct Label *l = (const struct Label *) tf->tf_regs.i1;
	tag_is_kobject(l, kobj_label);
	int r;
	r = label_compare_id(cur_mon_thread->th_ko.ko_label[kolabel_contaminate], l->lb_ko.ko_id, label_leq_starlo);
	if (r < 0) {
	    cprintf("MONCALL_SET_LABEL: too low\n");
	    //tag_print_label_id("Old label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
	    //tag_print_label_id("New label", l->lb_ko.ko_id);
	}

	r = label_compare_id(l->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	if (r < 0) {
	    cprintf("MONCALL_SET_LABEL: too high\n");
	    //tag_print_label_id("New label", l->lb_ko.ko_id);
	    //tag_print_label_id("Old clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	}

	put_retval = 1;
	retval = thread_change_label(cur_mon_thread, l);
	break;
    }

    case MONCALL_SET_CLEAR: {
	const struct Label *clear = (const struct Label *) tf->tf_regs.i1;
	tag_is_kobject(clear, kobj_label);

	const struct Label *cur_clear, *cur_label;
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_contaminate, &cur_label));
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_clearance, &cur_clear));

	struct Label *bound;
	assert(0 == label_max(cur_clear, cur_label, &bound, label_leq_starhi));

	int r = label_compare(clear, bound, label_leq_starhi, 0);
	if (r < 0) {
	    cprintf("MONCALL_SET_LABEL: too high\n");
	    //tag_print_label_id("Old label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
	    //tag_print_label_id("Old clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	    //tag_print_label_id("New clear", clear->lb_ko.ko_id);
	}

	put_retval = 1;
	retval = kobject_set_label(&kobject_dirty(&cur_mon_thread->th_ko)->hdr, kolabel_clearance, clear);
	break;
    }

    case MONCALL_CATEGORY_ALLOC: {
	put_retval = 1;
	uint64_t handle = handle_alloc();

	const struct Label *cur_label, *cur_clear;
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_contaminate, &cur_label));
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_clearance, &cur_clear));

	struct Label *l, *c;
	assert(0 == label_copy(cur_label, &l));
	assert(0 == label_set(l, handle, LB_LEVEL_STAR));

	assert(0 == label_copy(cur_clear, &c));
	assert(0 == label_set(c, handle, 3));

	// Prepare for changing the thread's clearance
	struct kobject_quota_resv qr;
	kobject_qres_init(&qr, &kobject_dirty(&cur_mon_thread->th_ko)->hdr);
	int r = kobject_qres_reserve(&qr, &c->lb_ko);
	if (r < 0) {
	    retval = r;
	    break;
	}

	// Change label, and changing clearance is now guaranteed to succeed
	r = thread_change_label(cur_mon_thread, l);
	if (r < 0) {
	    kobject_qres_release(&qr);
	    retval = r;
	    break;
	}
	kobject_set_label_prepared(&kobject_dirty(&cur_mon_thread->th_ko)->hdr,
				   kolabel_clearance, cur_clear, c, &qr);
	retval = handle;
	break;
    }

    case MONCALL_THREAD_START: {
	put_retval = 1;
	const struct Thread *t = (const struct Thread *) tf->tf_regs.i1;
	const struct Label *l = (const struct Label *) tf->tf_regs.i2;
	const struct Label *c = (const struct Label *) tf->tf_regs.i3;
	const struct thread_entry *te = (const struct thread_entry *) tf->tf_regs.i4;

	tag_is_kobject(t, kobj_thread);
	tag_is_kobject(l, kobj_label);
	tag_is_kobject(c, kobj_label);
	for (uint32_t i = 0; i < sizeof(*te); i += 4) {
	    uint32_t dt = read_dtag(((void *) te) + i);
	    assert(dt >= DTAG_STACK_0 && dt <= DTAG_STACK_EXL);
	}
	assert(SAFE_EQUAL(t->th_status, thread_not_started));

	int r;
	r = label_compare_id(cur_mon_thread->th_ko.ko_label[kolabel_contaminate], l->lb_ko.ko_id, label_leq_starlo);
	if (r < 0)
	    cprintf("MONCALL_THREAD_START: label too low\n");
	r = label_compare_id(c->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	if (r < 0)
	    cprintf("MONCALL_THREAD_START: clear too high\n");

	retval = thread_jump(t, l, c, te);
	break;
    }

    case MONCALL_GATE_ENTER: {
	put_retval = 1;
	const struct Gate *g = (const struct Gate *) tf->tf_regs.i1;
	const struct Label *l = (const struct Label *) tf->tf_regs.i2;
	const struct Label *c = (const struct Label *) tf->tf_regs.i3;
	const struct thread_entry *te = (const struct thread_entry *) tf->tf_regs.i4;

	tag_is_kobject(g, kobj_gate);
	tag_is_kobject(l, kobj_label);
	tag_is_kobject(c, kobj_label);
	for (uint32_t i = 0; i < sizeof(*te); i += 4) {
	    uint32_t dt = read_dtag(((void *) te) + i);
	    assert(dt >= DTAG_STACK_0 && dt <= DTAG_STACK_EXL);
	}

	const struct Label *cur_label, *cur_clear;
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_contaminate, &cur_label));
	assert(0 == kobject_get_label(&cur_mon_thread->th_ko, kolabel_clearance, &cur_clear));

	const struct Label *gl, *gc, *gv;
	assert(0 == kobject_get_label(&g->gt_ko, kolabel_contaminate, &gl));
	assert(0 == kobject_get_label(&g->gt_ko, kolabel_clearance, &gc));
	assert(0 == kobject_get_label(&g->gt_ko, kolabel_verify_contaminate, &gv));

	if (gv)
	    assert(0 == label_compare(cur_label, gv, label_leq_starlo, 0));

	struct Label *lb, *cb;
	assert(0 == label_max(gl, cur_label, &lb, label_leq_starhi));
	assert(0 == label_max(gc, cur_clear, &cb, label_leq_starlo));

	int r;
	r = label_compare(lb, l, label_leq_starlo, 0);
	if (r < 0)
	    cprintf("MONCALL_GATE_ENTER: label too low\n");

	r = label_compare(c, cb, label_leq_starhi, 0);
	if (r < 0)
	    cprintf("MONCALL_GATE_ENTER: clear too high\n");

	retval = thread_jump(cur_mon_thread, l, c, g->gt_te_unspec ? te : &g->gt_te);
	break;
    }

    default:
	panic("Unknown moncall type %d", tf->tf_regs.l0);
    }

    if (put_retval) {
	tf->tf_regs.i0 = (retval >> 32);
	tf->tf_regs.i1 = (retval & 0xffffffff);
    }

 out:
#if TAG_DEBUG
    prof_moncall(callnum, karch_get_tsc() - start);
#endif
    tag_trap_return(tf);
}

/*
 * Tag trap handling
 */

void
tag_trap(struct Trapframe *tf, uint32_t err, uint32_t errv)
{
#if TAG_DEBUG
    uint32_t start = karch_get_tsc();
#endif

    if (tag_trap_debug)
	cprintf("tag trap...\n");

    uint32_t et = read_et();
    uint32_t cause = (et >> ET_CAUSE_SHIFT) & ET_CAUSE_MASK;
    uint32_t dtag = read_etag();

    if (tag_trap_debug)
	cprintf("  data tag = %d, cause = %s (%d), pc = 0x%x\n",
		dtag,
		cause <= ET_CAUSE_EXEC ? cause_table[cause] : "unknown",
		cause, tf->tf_pc);

    if (err) {
	cprintf("  tag trap err = %d [%x]\n", err, errv);
	cprintf("  data tag = %d, cause = %d\n", dtag, cause);
	trapframe_print(tf);
	abort();
    }

    if (dtag == DTAG_MONCALL)
	tag_moncall(tf);

    int r = 0;
    switch (cause) {
    case ET_CAUSE_PCV:
	panic("Missing PC tag valid bits?!");

    case ET_CAUSE_DV:
	panic("Missing data tag valid bits?!");

    case ET_CAUSE_READ:
    case ET_CAUSE_EXEC:
	r = tag_compare(dtag, 0);
	break;

    case ET_CAUSE_WRITE:
	r = tag_compare(dtag, 1);
	break;

    default:
	panic("Unknown cause value from the ET register\n");
    }

    if (r < 0) {
	cprintf("tag compare @ 0x%x: dtag %d (%s): %s\n",
		tf->tf_pc, dtag, cause_table[cause], e2s(r));
	//tag_print_label_id("PC label", pctag_label_id[pctag]);
	//tag_print_label_id("Data label", dtag_label_id[dtag]);
	//trapframe_print(tf);

	write_tsr(read_tsr() | TSR_EO);
    }

    if (tag_trap_debug)
	cprintf("tag trap: returning..\n");
#if TAG_DEBUG
    prof_tagstuff(0, karch_get_tsc() - start);
#endif

    tag_trap_return(tf);
}

#if TAG_DEBUG
static void
periodic_pctag_show(void)
{
    cprintf("PC tag / dtag histogram:\n");
    for (uint32_t i = PCTAG_DYNAMIC; i < (1 << TAG_PC_BITS); i++) {
	if (pctag_dtag_count[i])
	    cprintf("%d->%d ", i, pctag_dtag_count[i]);
    }
    cprintf("\n");

    cprintf("%lld pages used, %lld kobjects\n",
	    page_stats.pages_used, nkobjects);

    monitor_call(MONCALL_FLUSHPERM);
}
#endif

void
tag_init_late(void)
{
#if TAG_DEBUG
    static struct periodic_task show_pt = {
	.pt_fn = &periodic_pctag_show, .pt_interval_sec = 10 };
    timer_add_periodic(&show_pt);
#endif
}

void
tag_init(void)
{
    write_rma((uintptr_t) &tag_trap_entry);
    write_tsr(TSR_T);

    cprintf("Initializing page tag table.. ");
    assert(global_npages <= sizeof(tag_table) / sizeof(tag_table[0]));
    for (uint32_t i = 0; i < sizeof(tag_table) / sizeof(tag_table[0]); i++)
	tag_table[i] = DTAG_NOACCESS;

    write_tb(kva2pa(&tag_table[0]));
    cprintf("[TB = %x] ", read_tb());

    tag_set(&stext[0],   DTAG_KEXEC, &etext[0]   - &stext[0]);
    tag_set(&srodata[0], DTAG_KRO,   &erodata[0] - &srodata[0]);
    tag_set(&sdata[0],   DTAG_KRO,   &edata[0]   - &sdata[0]);
    tag_set(&srwdata[0], DTAG_KRW,   &erwdata[0] - &srwdata[0]);
    tag_set(&sbss[0],    DTAG_KRO,   &ebss[0]    - &sbss[0]);
    tag_set(&kstack[0],  DTAG_STACK_0, KSTACK_SIZE);

    tag_set(&moncall_dummy, DTAG_MONCALL, sizeof(moncall_dummy));

    /* keep the hardware happy? */
    write_pctag(0);
    wrtpcv(0, 1);
    tag_flushperm();

    cprintf("done.\n");

    cprintf("Initializing stacks.. ");
    assert(KSTACK_SIZE * PCALL_DEPTH == (extra_stack_top - extra_stack));
    assert(DTAG_STACK_EXL - DTAG_STACK_EX + 1 == PCALL_DEPTH);
    for (uint32_t i = 0; i < PCALL_DEPTH; i++) {
	pcall_stack[i].stack_base = (uint8_t *) &extra_stack[i * KSTACK_SIZE];
	pcall_stack[i].stack_top  = (uint8_t *) &extra_stack[(i + 1) * KSTACK_SIZE];
	tag_set(&extra_stack[i * KSTACK_SIZE], DTAG_STACK_EX + i, KSTACK_SIZE);
    }
    cur_stack_base = (uintptr_t) &kstack[0];
    cprintf("done.\n");
}

void
tag_is_kobject(const void *ptr, uint8_t type)
{
    assert(!PGOFF(ptr));
    assert(DTAG_TYPE_KOBJ == read_dtag(ptr));

    const struct kobject_hdr *ko = ptr;
    assert(type == kobj_any || ko->ko_type == type);
}

void
tag_is_syncslot(const void *ptr)
{
    for (uint32_t i = 0; i < sizeof(struct sync_wait_slot); i += 4) {
	//assert(DTAG_TYPE_SYNC == read_dtag(ptr + i));
    }
}
