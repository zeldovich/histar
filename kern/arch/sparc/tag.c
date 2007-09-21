#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <machine/trap.h>
#include <kern/lib.h>
#include <kern/arch.h>

extern const uint8_t stext[], erodata[];
extern const uint8_t kstack[], kstack_top[];
extern const uint8_t monstack[], monstack_top[];

const char* cause_table[] = {
    [ET_CAUSE_PCV]   = "PC tag invalid",
    [ET_CAUSE_DV]    = "Data tag invalid",
    [ET_CAUSE_READ]  = "Read permission",
    [ET_CAUSE_WRITE] = "Write permission",
    [ET_CAUSE_EXEC]  = "Execute permission",
};

void
tag_set(const void *addr, uint32_t dtag, size_t n)
{
    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));

    for (uint32_t i = 0; i < n; i += 4)
	write_dtag(addr + i, dtag);
}

void
tag_trap(struct Trapframe *tf, uint32_t tbr, uint32_t err, uint32_t errv)
{
    cprintf("tag trap...\n");

    uint32_t pctag = read_pctag();
    uint32_t et = read_et();
    uint32_t cause = (et >> ET_CAUSE_SHIFT) & ET_CAUSE_MASK;
    uint32_t dtag = (et >> ET_TAG_SHIFT) & ET_TAG_MASK;

    cprintf("  pc tag = %d\n", pctag);
    cprintf("  d tag  = %d\n", dtag);
    cprintf("  cause  = %s (%d)\n",
	    cause <= ET_CAUSE_EXEC ? cause_table[cause] : "unknown", cause);
    cprintf("  tf     = %p\n", tf);
    cprintf("  pc     = %x\n", tf->tf_pc);
    cprintf("  psr    = %x\n", tf->tf_psr);

    trapframe_print(tf);

    if (err) {
	cprintf("  err = %d [%x]\n", err, errv);
	trapframe_print(tf);
	abort();
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
	wrtperm(pctag, dtag, TAG_PERM_EXEC);
	break;

    default:
	panic("Unknown cause value from the ET register\n");
    }

    cprintf("tag trap: returning..\n");
    tag_trap_return(tf, tbr);
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
	wrtperm(i, DTAG_KERNEL_RO, TAG_PERM_READ | TAG_PERM_EXEC);
	wrtperm(i, DTAG_KSTACK, TAG_PERM_READ | TAG_PERM_WRITE);

	/*
	 * Currently we don't properly tag all of our memory pages,
	 * so in order to access all of the dynamically allocated
	 * memory, kobjects, user segments, and so on, we need this..
	 */
	wrtperm(i, DTAG_NOACCESS,
		TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);
    }

    write_pctag(0);
    cprintf("done.\n");

    cprintf("Initializing memory tags.. ");
    tag_set(pa2kva(ppn2pa(0)), DTAG_NOACCESS, global_npages * PGSIZE);
    tag_set(&stext[0], DTAG_KERNEL_RO, &erodata[0] - &stext[0]);
    tag_set(&kstack[0], DTAG_KSTACK, KSTACK_SIZE);
    cprintf("done.\n");

    cprintf("Disabling trusted mode.. ");
    write_tsr(0);
    cprintf("done.\n");
}
