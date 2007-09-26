#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <machine/trap.h>
#include <machine/psr.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/label.h>

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
    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));

    for (uint32_t i = 0; i < n; i += 4)
	write_dtag(addr + i, dtag);
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

    switch (tf->tf_regs.l0) {
    case MONCALL_PCALL:
	if (pcall_next == PCALL_DEPTH)
	    panic("MONCALL_PCALL: out of pstack space\n");

	ps = &pcall_stack[pcall_next++];
	memcpy(&ps->tf, tf, sizeof(*tf));
	ps->pctag = read_pctag();
	ps->prev_stack_base = cur_stack_base;

	write_pctag(tf->tf_regs.l1);
	tag_set(ps->stack_base, tf->tf_regs.l2, KSTACK_SIZE);

	tf->tf_pc = (uintptr_t) &moncall_trampoline;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_psr = PSR_S | PSR_PS | PSR_PIL | PSR_ET;
	tf->tf_wim = 2;
	tf->tf_regs.sp = ((uintptr_t) ps->stack_top) - STACKFRAME_SZ;
	tf->tf_regs.fp = 0;
	cur_stack_base = (uintptr_t) ps->stack_base;
	break;

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

    default:
	panic("Unknown moncall type %d", tf->tf_regs.l0);
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

    if (dtag < DTAG_DYNAMIC) {
	cprintf("non-dynamic tag fault: pctag %d, dtag %d, cause %s\n",
		pctag, dtag, cause_table[cause]);
	trapframe_print(tf);
    }

    switch (cause) {
    case ET_CAUSE_PCV:
	panic("Missing PC tag valid bits?!");

    case ET_CAUSE_DV:
	panic("Missing data tag valid bits?!");

    case ET_CAUSE_READ:
	wrtperm(pctag, dtag, TAG_PERM_READ);
	break;

    case ET_CAUSE_WRITE:
	wrtperm(pctag, dtag, TAG_PERM_READ | TAG_PERM_WRITE);
	break;

    case ET_CAUSE_EXEC:
	wrtperm(pctag, dtag, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
	break;

    default:
	panic("Unknown cause value from the ET register\n");
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
	    wrtperm(i, j, 0);

	wrtperm(i, DTAG_DEVICE, TAG_PERM_READ | TAG_PERM_WRITE);
	wrtperm(i, DTAG_KEXEC, TAG_PERM_READ | TAG_PERM_EXEC);
	wrtperm(i, DTAG_KRO, TAG_PERM_READ);
	wrtperm(i, DTAG_KRW, TAG_PERM_READ | TAG_PERM_WRITE);
    }

    write_pctag(PCTAG_DYNAMIC);
    cprintf("done.\n");

    cprintf("Initializing memory tags.. ");
    tag_set(pa2kva(ppn2pa(0)), DTAG_NOACCESS, global_npages * PGSIZE);

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

static uint64_t dtag_label_id[1 << TAG_DATA_BITS];
static uint64_t pctag_label_id[1 << TAG_PC_BITS];

const struct Label dtag_label[DTAG_DYNAMIC];

uint32_t
tag_alloc(const struct Label *l, int tag_type)
{
    assert(l);

    if (tag_type == tag_type_data) {
	if (l >= &dtag_label[0] && l < &dtag_label[DTAG_DYNAMIC]) {
	    uintptr_t lp = (uintptr_t) l;
	    uintptr_t l0 = (uintptr_t) &dtag_label[0];
	    return (lp - l0) / sizeof(struct Label);
	}

	uint32_t maxtag = (1 << TAG_DATA_BITS);
	if (l->lb_dtag_hint < maxtag &&
	    dtag_label_id[l->lb_dtag_hint] == l->lb_ko.ko_id)
	{
	    return l->lb_dtag_hint;
	}

	for (uint32_t i = DTAG_DYNAMIC; i < maxtag; i++) {
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

	for (uint32_t i = PCTAG_DYNAMIC; i < maxtag; i++) {
	    if (pctag_label_id[i] == 0) {
		pctag_label_id[i] = l->lb_ko.ko_id;
		kobject_ephemeral_dirty(&l->lb_ko)->lb.lb_pctag_hint = i;
		return i;
	    }
	}

	panic("tag_alloc: out of pc tags");
    }

    panic("tag_alloc: bad tag type %d", tag_type);
}
