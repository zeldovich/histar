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
tag_trap(void)
{
    panic("tag_trap");
}

void
tag_init(void)
{
    for (uint32_t i = 0; i < global_npages; i++)
	tag_set(pa2kva(ppn2pa(i)), DTAG_NOACCESS, PGSIZE);

    for (void *p = (void *) &stext[0]; p < (void *) &erodata[0]; p += 4)
	write_dtag(p, DTAG_KERNEL_RO);

    

    cprintf("Tag testing..\n");

    static uint32_t v;
    cprintf("vp = %p\n", &v);
    cprintf("v tag = %d\n", read_dtag(&v));
    write_dtag(&v, 6);
    cprintf("v tag = %d\n", read_dtag(&v));

    cprintf("pc tag = %d\n", read_pctag());
    write_pctag(7);
    cprintf("pc tag = %d\n", read_pctag());

    cprintf("tsr = 0x%x\n", read_tsr());

    cprintf("rma = 0x%x\n", read_rma());
    write_rma(&tag_trap);
    cprintf("rma = 0x%x\n", read_rma());

    write_tsr(0);
    cprintf("tsr = 0x%x\n", read_tsr());
}
