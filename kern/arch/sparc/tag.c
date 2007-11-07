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
#include <inc/error.h>

extern const uint8_t stext[],   etext[];
extern const uint8_t srodata[], erodata[];
extern const uint8_t sdata[],   edata[];
extern const uint8_t srwdata[], erwdata[];
extern const uint8_t sbss[],    ebss[];

extern const uint8_t kstack[], kstack_top[];
extern const uint8_t monstack[], monstack_top[];
extern const uint8_t extra_stack[], extra_stack_top[];

uint32_t moncall_dummy;
uint32_t cur_stack_base;

enum { tag_trap_debug = 0 };

static uint8_t tag_permtable[1 << TAG_PC_BITS][1 << TAG_DATA_BITS];
static uint64_t dtag_refcount[1 << TAG_DATA_BITS];

static uint64_t dtag_label_id[1 << TAG_DATA_BITS];
static uint64_t pctag_label_id[1 << TAG_PC_BITS];
static const struct Label *pctag_label[1 << TAG_PC_BITS];

const struct Label dtag_label[DTAG_DYNAMIC];
const struct Thread *cur_mon_thread;

const char* const cause_table[] = {
    [ET_CAUSE_PCV]   = "PC tag invalid",
    [ET_CAUSE_DV]    = "Data tag invalid",
    [ET_CAUSE_READ]  = "Read",
    [ET_CAUSE_WRITE] = "Write",
    [ET_CAUSE_EXEC]  = "Execute",
};

void
tag_set(const void *addr, uint32_t dtag, size_t n)
{
    if (!(read_tsr() & TSR_T)) {
	int r = monitor_call(MONCALL_TAGSET, addr, dtag, n);
	if (r != 0)
	    cprintf("tag_set: moncall_tagset: %s (%d)\n", e2s(r), r);
	return;
    }

    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));
    assert(!(n & 3));

    for (uint32_t i = 0; i < n; i += 4) {
	uint32_t old_tag = read_dtag(addr + i);
	dtag_refcount[old_tag & ((1 << TAG_DATA_BITS) - 1)]--;
	write_dtag(addr + i, dtag);
    }

    dtag_refcount[dtag] += n / 4;
}

uint32_t
tag_getperm(uint32_t pctag, uint32_t dtag)
{
    return tag_permtable[pctag][dtag];
}

void
tag_setperm(uint32_t pctag, uint32_t dtag, uint32_t perm)
{
    tag_permtable[pctag][dtag] = perm;
    wrtperm(pctag, dtag, perm);
}

/*
 * Tag comparison logic
 */

static int
tag_compare(uint32_t pctag, uint32_t dtag, int write)
{
    int r = label_compare_id(dtag_label_id[dtag],
			     pctag_label_id[pctag],
			     label_leq_starhi);
    if (r < 0)
	return r;

    if (write) {
	r = label_compare_id(pctag_label_id[pctag],
			     dtag_label_id[dtag],
			     label_leq_starlo);
	if (r < 0)
	    return r;

	tag_setperm(pctag, dtag, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
    } else {
	tag_setperm(pctag, dtag, TAG_PERM_READ | TAG_PERM_EXEC);
    }

    return 0;
}

static void
tag_print_label_id(const char *msg, uint64_t id)
{
    const struct Label *l;
    int r = kobject_get(id, (const struct kobject **) &l,
			kobj_label, iflow_none);
    if (r < 0) {
	cprintf("%s %"PRIu64": %s\n", msg, id, e2s(r));
	return;
    }

    cprintf("%s %"PRIu64": ", msg, id);
    label_cprint(l);
}

/*
 * Monitor call support
 */

struct pcall_stack {
    uint8_t *stack_base;
    uint8_t *stack_top;
    struct Trapframe tf;
    uint32_t pctag;
    uintptr_t prev_stack_base;
};

static struct pcall_stack pcall_stack[PCALL_DEPTH];
static uint32_t pcall_next;

static int32_t
moncall_tagset(void *addr, uint32_t dtag, uint32_t nbytes)
{
    if (((uintptr_t) addr) & 3)
	return -E_INVAL;

    if (dtag == DTAG_TYPE_KOBJ || dtag == DTAG_TYPE_SYNC)
	return -E_INVAL;

    uint32_t pctag = read_pctag();
    uint32_t pbits = TAG_PERM_READ | TAG_PERM_WRITE;
    uint32_t last_dtag = ~0;
    for (uint32_t i = 0; i < nbytes; i += 4) {
	uint32_t old_dtag = read_dtag(addr + i);
	if (old_dtag != last_dtag) {
	    uint32_t perm = tag_getperm(pctag, old_dtag);
	    if ((perm & pbits) != pbits) {
		tag_compare(pctag, old_dtag, 1);
		perm = tag_getperm(pctag, old_dtag);
		if (!(perm & pbits) != pbits) {
		    cprintf("moncall_tagset: err %p pctag=%d old dtag=%d new dtag=%d\n",
			    addr + i, pctag, old_dtag, dtag);
		    return -E_LABEL;
		}
	    }
	    last_dtag = old_dtag;
	}

	write_dtag(addr + i, dtag);
	dtag_refcount[old_dtag]--;
	dtag_refcount[dtag]++;
    }

    return 0;
}

static void __attribute__((noreturn))
tag_moncall(struct Trapframe *tf)
{
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

    switch (tf->tf_regs.l0) {
    case MONCALL_PCALL:
	assert(pcall_next < PCALL_DEPTH);

	ps = &pcall_stack[pcall_next++];
	memcpy(&ps->tf, tf, sizeof(*tf));
	ps->pctag = read_pctag();
	ps->prev_stack_base = cur_stack_base;

	write_pctag(tf->tf_regs.l1);
	tag_set(ps->stack_base, tf->tf_regs.l2, KSTACK_SIZE);

	tf->tf_pc = (uintptr_t) &pcall_trampoline;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.sp = ((uintptr_t) ps->stack_top) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	cur_stack_base = (uintptr_t) ps->stack_base;
	break;

    case MONCALL_KOBJ_GC: {
	const struct kobject *ko = (const struct kobject *) tf->tf_regs.i1;

	tag_is_kobject(ko, kobj_any);
	if (ko->hdr.ko_type == kobj_label) {
	    put_retval = 1;
	    retval = kobject_gc(kobject_dirty(&ko->hdr));
	    break;
	}

	assert(pcall_next < PCALL_DEPTH);
	ps = &pcall_stack[pcall_next++];
	memcpy(&ps->tf, tf, sizeof(*tf));
	ps->pctag = read_pctag();
	ps->prev_stack_base = cur_stack_base;

	const struct Label *l;
	assert(0 == kobject_get_label(&ko->hdr, kolabel_contaminate, &l));
	uint32_t pctag = tag_alloc(l, tag_type_pc);

	write_pctag(pctag);
	tag_set(ps->stack_base, DTAG_KRW, KSTACK_SIZE);

	tf->tf_pc = (uintptr_t) &pcall_trampoline;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.l3 = (uintptr_t) &kobject_gc;
	tf->tf_regs.i0 = (uintptr_t) ko;
	tf->tf_regs.sp = ((uintptr_t) ps->stack_top) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	cur_stack_base = (uintptr_t) ps->stack_base;
	break;
    }

    case MONCALL_PRETURN:
	if (pcall_next == 0)
	    panic("MONCALL_PRETURN: nothing on the pstack\n");

	ps = &pcall_stack[--pcall_next];
	ps->tf.tf_regs.i0 = tf->tf_regs.o0;
	ps->tf.tf_regs.i1 = tf->tf_regs.o1;
	tf = &ps->tf;
	write_pctag(ps->pctag);
	cur_stack_base = ps->prev_stack_base;
	break;

    case MONCALL_TAGSET:
	put_retval = 1;
	retval = moncall_tagset((void *) tf->tf_regs.i1,
				tf->tf_regs.i2,
				tf->tf_regs.i3);
	break;

    case MONCALL_DTAGALLOC: {
	put_retval = 1;
	retval = -E_NO_MEM;
	uint64_t label_id = ((uint64_t) tf->tf_regs.i1) << 32 | tf->tf_regs.i2;
	for (uint32_t i = DTAG_DYNAMIC; i < (1 << TAG_DATA_BITS); i++) {
	    if (dtag_refcount[i] == 0 && dtag_label_id[i]) {
		for (uint32_t j = PCTAG_DYNAMIC; j < (1 << TAG_PC_BITS); j++)
		    if (tag_getperm(j, i))
			tag_setperm(j, i, 0);
		dtag_label_id[i] = 0;
	    }

	    if (dtag_label_id[i] == 0) {
		dtag_label_id[i] = label_id;
		retval = i;
		break;
	    }
	}
	break;
    }

    case MONCALL_THREAD_SWITCH: {
	const struct Thread *tptr = (const struct Thread *) tf->tf_regs.i1;
	assert(tptr);
	tag_is_kobject(tptr, kobj_thread);
	cur_thread = tptr;
	cur_mon_thread = tptr;

	const struct Label *tl;
	int r = kobject_get_label(&tptr->th_ko, kolabel_contaminate, &tl);
	assert(r >= 0);

	uint32_t pctag = tag_alloc(tl, tag_type_pc);
	write_pctag(pctag);

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
	assert(read_dtag(kp) == DTAG_KRW);

	if (l && cur_mon_thread) {
	    int r;
	    r = label_compare_id(cur_mon_thread->th_ko.ko_label[kolabel_contaminate], l->lb_ko.ko_id, label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object label too low\n");
		tag_print_label_id("Thread label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
		tag_print_label_id("Object label", l->lb_ko.ko_id);
	    }

	    r = label_compare_id(l->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object label too high\n");
		tag_print_label_id("Object label", l->lb_ko.ko_id);
		tag_print_label_id("Thread clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	    }
	}

	if (clear && cur_mon_thread) {
	    int r;
	    r = label_compare_id(clear->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	    if (r < 0) {
		cprintf("MONCALL_KOBJ_ALLOC: object clearance too high\n");
		tag_print_label_id("Object clear", clear->lb_ko.ko_id);
		tag_print_label_id("Thread clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
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
	    tag_print_label_id("Old label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
	    tag_print_label_id("New label", l->lb_ko.ko_id);
	}

	r = label_compare_id(l->lb_ko.ko_id, cur_mon_thread->th_ko.ko_label[kolabel_clearance], label_leq_starlo);
	if (r < 0) {
	    cprintf("MONCALL_SET_LABEL: too high\n");
	    tag_print_label_id("New label", l->lb_ko.ko_id);
	    tag_print_label_id("Old clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
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
	    tag_print_label_id("Old label", cur_mon_thread->th_ko.ko_label[kolabel_contaminate]);
	    tag_print_label_id("Old clear", cur_mon_thread->th_ko.ko_label[kolabel_clearance]);
	    tag_print_label_id("New clear", clear->lb_ko.ko_id);
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
	for (uint32_t i = 0; i < sizeof(*te); i += 4)
	    assert(read_dtag(((void *)te) + i) == DTAG_KRW);
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
	for (uint32_t i = 0; i < sizeof(*te); i += 4)
	    assert(read_dtag(((void *)te) + i) == DTAG_KRW);

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
    tag_trap_return(tf);
}

/*
 * Tag trap handling
 */

void
tag_trap(struct Trapframe *tf, uint32_t err, uint32_t errv)
{
    if (tag_trap_debug)
	cprintf("tag trap...\n");

    uint32_t pctag = read_pctag();
    uint32_t et = read_et();
    uint32_t cause = (et >> ET_CAUSE_SHIFT) & ET_CAUSE_MASK;
    uint32_t dtag = (et >> ET_TAG_SHIFT) & ET_TAG_MASK;

    if (tag_trap_debug)
	cprintf("  pc tag = %d, data tag = %d, cause = %s (%d), pc = 0x%x\n",
		pctag, dtag,
		cause <= ET_CAUSE_EXEC ? cause_table[cause] : "unknown",
		cause, tf->tf_pc);

    if (err) {
	cprintf("  tag trap err = %d [%x]\n", err, errv);
	cprintf("  pc tag = %d, data tag = %d, cause = %d\n",
		pctag, dtag, cause);
	trapframe_print(tf);
	abort();
    }

    if (dtag == DTAG_MONCALL)
	tag_moncall(tf);

    int skip = 0;
    if (dtag < DTAG_DYNAMIC) {
	cprintf("non-dynamic tag fault @ 0x%x: pctag %d, dtag %d (%s)\n",
		tf->tf_pc, pctag, dtag, cause_table[cause]);
	//trapframe_print(tf);

	skip = 1;
	write_tsr(read_tsr() | TSR_EO);
    }

    int r = 0;
    switch (cause) {
    case ET_CAUSE_PCV:
	panic("Missing PC tag valid bits?!");

    case ET_CAUSE_DV:
	panic("Missing data tag valid bits?!");

    case ET_CAUSE_READ:
    case ET_CAUSE_EXEC:
	r = skip ? 0 : tag_compare(pctag, dtag, 0);
	break;

    case ET_CAUSE_WRITE:
	r = skip ? 0 : tag_compare(pctag, dtag, 1);
	break;

    default:
	panic("Unknown cause value from the ET register\n");
    }

    if (r < 0) {
	cprintf("tag compare @ 0x%x: pctag %d, dtag %d (%s): %s\n",
		tf->tf_pc, pctag, dtag, cause_table[cause], e2s(r));
	tag_print_label_id("PC label", pctag_label_id[pctag]);
	tag_print_label_id("Data label", dtag_label_id[dtag]);
	//trapframe_print(tf);

	write_tsr(read_tsr() | TSR_EO);
    }

    if (tag_trap_debug)
	cprintf("tag trap: returning..\n");
    tag_trap_return(tf);
}

void
tag_init(void)
{
    write_rma((uintptr_t) &tag_trap_entry);
    write_tsr(TSR_T);

    cprintf("Initializing tag permission table.. ");

    /* Per-tag valid bits appear to be useless */
    for (uint32_t i = 0; i < (1 << TAG_PC_BITS); i++)
	wrtpcv(i, 1);

    for (uint32_t i = 0; i < (1 << TAG_DATA_BITS); i++)
	wrtdv(i, 1);

    for (uint32_t i = 0; i < (1 << TAG_PC_BITS); i++) {
	for (uint32_t j = 0; j < (1 << TAG_DATA_BITS); j++)
	    tag_setperm(i, j, 0);

	tag_setperm(i, DTAG_DEVICE, TAG_PERM_READ | TAG_PERM_WRITE);
	tag_setperm(i, DTAG_KEXEC, TAG_PERM_READ | TAG_PERM_EXEC);
	tag_setperm(i, DTAG_KRO, TAG_PERM_READ);
	tag_setperm(i, DTAG_KRW, TAG_PERM_READ | TAG_PERM_WRITE);
	tag_setperm(i, DTAG_TYPE_KOBJ, TAG_PERM_READ);
	tag_setperm(i, DTAG_TYPE_SYNC, TAG_PERM_READ | TAG_PERM_WRITE);
    }

    for (uint32_t i = 0; i < (1 << TAG_DATA_BITS); i++)
	tag_setperm(PCTAG_INIT, i,
		    TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
    tag_setperm(PCTAG_INIT, DTAG_MONCALL, 0);

    write_pctag(PCTAG_INIT);
    cprintf("done.\n");

    cprintf("Initializing memory tags.. ");
    tag_set(pa2kva(ppn2pa(0)), DTAG_NOACCESS, global_npages * PGSIZE);

    /* Normalize refcounts */
    for (uint32_t i = 0; i < (1 << TAG_DATA_BITS); i++)
	dtag_refcount[i] = 0;
    dtag_refcount[DTAG_NOACCESS] = global_npages * PGSIZE / 4;

    tag_set(&stext[0],   DTAG_KEXEC, &etext[0]   - &stext[0]);
    tag_set(&srodata[0], DTAG_KRO,   &erodata[0] - &srodata[0]);
    tag_set(&sdata[0],   DTAG_KRO,   &edata[0]   - &sdata[0]);
    tag_set(&srwdata[0], DTAG_KRW,   &erwdata[0] - &srwdata[0]);
    tag_set(&sbss[0],    DTAG_KRO,   &ebss[0]    - &sbss[0]);
    tag_set(&kstack[0],  DTAG_KRW,   KSTACK_SIZE);

    tag_set(&moncall_dummy, DTAG_MONCALL, sizeof(moncall_dummy));
    cprintf("done.\n");

    cprintf("Initializing stacks.. ");
    assert(KSTACK_SIZE * PCALL_DEPTH == (extra_stack_top - extra_stack));
    for (uint32_t i = 0; i < PCALL_DEPTH; i++) {
	pcall_stack[i].stack_base = (uint8_t *) &extra_stack[i * KSTACK_SIZE];
	pcall_stack[i].stack_top  = (uint8_t *) &extra_stack[(i + 1) * KSTACK_SIZE];
    }
    cur_stack_base = (uintptr_t) &kstack[0];
    cprintf("done.\n");
}

uint32_t
tag_alloc(const struct Label *l, int tag_type)
{
    assert(l);
    if (l < &dtag_label[0] || l >= &dtag_label[DTAG_DYNAMIC])
	tag_is_kobject(l, kobj_label);

    if (tag_type == tag_type_data) {
	if (l >= &dtag_label[0] && l < &dtag_label[DTAG_DYNAMIC]) {
	    uintptr_t lp = (uintptr_t) l;
	    uintptr_t l0 = (uintptr_t) &dtag_label[0];
	    return (lp - l0) / sizeof(struct Label);
	}

	uint32_t maxtag = (1 << TAG_DATA_BITS);
	uint32_t hint = l->lb_dtag_hint;
	if (hint < maxtag && dtag_label_id[hint] == l->lb_ko.ko_id)
	    return hint;

	if (!(read_tsr() & TSR_T)) {
	    int r = monitor_call(MONCALL_DTAGALLOC, l->lb_ko.ko_id);
	    if (r < 0)
		cprintf("tag_alloc: moncall_dtagalloc: %s (%d)\n", e2s(r), r);
	    kobject_ephemeral_dirty(&l->lb_ko)->lb.lb_dtag_hint = r;
	    return r;
	}

	for (uint32_t i = DTAG_DYNAMIC; i < maxtag; i++) {
	    if (dtag_refcount[i] == 0 && dtag_label_id[i]) {
		for (uint32_t j = PCTAG_DYNAMIC; j < (1 << TAG_PC_BITS); j++)
		    if (tag_getperm(j, i))
			tag_setperm(j, i, 0);
		dtag_label_id[i] = 0;
	    }

	    if (dtag_label_id[i] == 0) {
		dtag_label_id[i] = l->lb_ko.ko_id;
		kobject_ephemeral_dirty(&l->lb_ko)->lb.lb_dtag_hint = i;
		return i;
	    }
	}

	panic("tag_alloc: out of data tags");
    }

    if (tag_type == tag_type_pc) {
	uint32_t maxtag = (1 << TAG_PC_BITS);
	if (l->lb_pctag_hint < maxtag &&
	    pctag_label_id[l->lb_pctag_hint] == l->lb_ko.ko_id)
	{
	    return l->lb_pctag_hint;
	}

 pc_retry:
	for (uint32_t i = PCTAG_DYNAMIC; i < maxtag; i++) {
	    if (pctag_label_id[i] == 0) {
		pctag_label_id[i] = l->lb_ko.ko_id;
		pctag_label[i] = l;
		kobject_pin_hdr(&l->lb_ko);

		kobject_ephemeral_dirty(&l->lb_ko)->lb.lb_pctag_hint = i;
		return i;
	    }
	}

	cprintf("Out of PC tags, flushing table..\n");
	for (uint32_t i = PCTAG_DYNAMIC; i < maxtag; i++) {
	    if (pctag_label[i])
		kobject_unpin_hdr(&pctag_label[i]->lb_ko);
	    pctag_label[i] = 0;
	    pctag_label_id[i] = 0;

	    for (uint32_t j = DTAG_DYNAMIC; j < (1 << TAG_DATA_BITS); j++)
		if (tag_getperm(i, j))
		    tag_setperm(i, j, 0);
	}

	goto pc_retry;
    }

    panic("tag_alloc: bad tag type %d", tag_type);
}

void
tag_is_kobject(const void *ptr, uint8_t type)
{
    assert(!PGOFF(ptr));
    //assert(DTAG_TYPE_KOBJ == read_dtag(ptr));

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
