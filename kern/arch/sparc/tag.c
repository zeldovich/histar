#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <kern/lib.h>

void
tag_set(const void *addr, uint32_t dtag, size_t n)
{
    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));

    for (uint32_t i = 0; i < n; i += 4)
	write_dtag(ptr + i, dtag);
}

void
tag_trap(void)
{
    panic("tag_trap");
}

void
tag_init(void)
{
    cprintf("Tag testing..\n");

    static uint32_t v;
    uintptr_t vp = (uintptr_t) &v;
    cprintf("vp = 0x%x\n", vp);
    cprintf("v tag = %d\n", read_dtag(vp));
    write_dtag(vp, 6);
    cprintf("v tag = %d\n", read_dtag(vp));

    cprintf("pc tag = %d\n", read_pctag());
    write_pctag(7);
    cprintf("pc tag = %d\n", read_pctag());

    cprintf("tsr = 0x%x\n", read_tsr());

    cprintf("rma = 0x%x\n", read_rma());
    write_rma(0xdeadbeef);
    cprintf("rma = 0x%x\n", read_rma());

    write_tsr(0);
    cprintf("tsr = 0x%x\n", read_tsr());
}
