#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <kern/lib.h>
#include <kern/arch.h>

extern const uint8_t stext[], erodata[];
extern const uint8_t kstack[], kstack_top[];
extern const uint8_t monstack[], monstack_top[];

void
tag_set(const void *addr, uint32_t dtag, size_t n)
{
    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));

    for (uint32_t i = 0; i < n; i += 4)
	write_dtag(addr + i, dtag);
}

void
tag_trap(struct Trapframe *tf, uint32_t tbr)
{
    cprintf("tag trap...\n");

    uint32_t pctag = read_pctag();
    uint32_t et = read_et();
    uint32_t cause = (et >> ET_CAUSE_SHIFT) & ET_CAUSE_MASK;
    uint32_t dtag = (et >> ET_TAG_SHIFT) & ET_TAG_MASK;

    cprintf("  pc tag = %d\n", pctag);
    cprintf("  d tag  = %d\n", dtag);
    cprintf("  cause  = %d\n", cause);
    cprintf("  tsr = 0x%x\n", read_tsr());
    cprintf("  tf  = %p\n", tf);
    cprintf("  pc  = 0x%x\n", tf->tf_pc);

    switch (cause) {
    case ET_CAUSE_PCV:
    case ET_CAUSE_DV:
	panic("Missing PC/data tag valid bits?!");

    case ET_CAUSE_READ:
	wrtperm(pctag, dtag, TAG_PERM_READ);
	break;

    case ET_CAUSE_WRITE:
	wrtperm(pctag, dtag, TAG_PERM_READ | TAG_PERM_WRITE);
	break;

    case ET_CAUSE_EXEC:
	wrtperm(pctag, dtag, TAG_PERM_EXEC | TAG_PERM_READ);
	break;

    default:
	panic("Unknown cause value from the ET register\n");
    }

    for (uint32_t i = 0; i < (1 << TAG_PC_BITS); i++)
	for (uint32_t j = 0; j < (1 << TAG_DATA_BITS); j++)
	    wrtperm(i, j, TAG_PERM_READ | TAG_PERM_WRITE | TAG_PERM_EXEC);

    static int count = 0;
    count++;
    if (count == 4) {
	cprintf("enough traps\n");
	abort();
    }

    tag_trap_return(tf, tbr);
}

volatile uint32_t v;

void
tag_init(void)
{
    write_rma((uintptr_t) &tag_trap_entry);

    cprintf("Initializing tag permission table.. ");

    /* Per-tag valid bits appear to be useless */
    for (uint32_t i = 0; i < (1 << TAG_PC_BITS); i++)
	wrtpcv(i, 1);

    for (uint32_t i = 0; i < (1 << TAG_DATA_BITS); i++)
	wrtdv(i, 1);

    for (uint32_t i = 0; i < (1 << TAG_PC_BITS); i++)
	for (uint32_t j = 0; j < (1 << TAG_DATA_BITS); j++)
	    wrtperm(i, j, 0);
    cprintf("done.\n");

    cprintf("Initializing memory tags.. ");
    for (uint32_t i = 0; i < global_npages; i++)
	tag_set(pa2kva(ppn2pa(i)), DTAG_NOACCESS, PGSIZE);

    for (void *p = (void *) &stext[0]; p < (void *) &erodata[0]; p += 4)
	write_dtag(p, DTAG_KERNEL_RO);
    cprintf("done.\n");

    cprintf("Tag testing..\n");

    cprintf("vp = %p\n", &v);
    cprintf("v tag = %d\n", read_dtag((void *) &v));
    write_dtag((void *) &v, 6);
    cprintf("v tag = %d\n", read_dtag((void *) &v));

    cprintf("pc tag = %d\n", read_pctag());
    write_pctag(2);
    cprintf("pc tag = %d\n", read_pctag());

    cprintf("tsr = 0x%x\n", read_tsr());

    cprintf("about to cause tag traps...\n");
    write_tsr(0);
    v = 5;

    cprintf("tsr = 0x%x\n", read_tsr());
}
