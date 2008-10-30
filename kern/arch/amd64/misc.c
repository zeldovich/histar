#include <machine/x86.h>
#include <kern/arch.h>
#include <inc/error.h>

uint32_t
arch_cpu(void)
{
    return (KSTACKTOP(0) - read_rsp()) / (3 * PGSIZE);
}

uintptr_t
karch_get_sp(void)
{
    return read_rsp();
}

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    jb->jb_rip = (uintptr_t) fn;
    jb->jb_rsp = (uintptr_t) ROUNDUP(stackbase, PGSIZE);
}

int
arch_out_port(uint64_t port, uint8_t width, uint8_t *val, uint64_t n)
{
    switch (width) {
    case 1:
	outsb(port, val, n);
	return 0;
    case 2:
	outsw(port, val, n);
	return 0;
    case 4:
	outsl(port, val, n);
	return 0;
    default:
	return -E_INVAL;
    }
}

int
arch_in_port(uint64_t port, uint8_t width, uint8_t *val, uint64_t n)
{
    switch (width) {
    case 1:
	insb(port, val, n);
	return 0;
    case 2:
	insw(port, val, n);
	return 0;
    case 4:
	insl(port, val, n);
	return 0;
    default:
	return -E_INVAL;
    }
}
