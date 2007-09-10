#include <machine/types.h>
#include <machine/tag.h>
#include <machine/sparc-tag.h>
#include <machine/tagutil.h>
#include <kern/lib.h>

void
tagset(const void *addr, uint32_t dtag, size_t n)
{
    uintptr_t ptr = (uintptr_t) addr;
    assert(!(ptr & 3));

    for (uint32_t i = 0; i < n; i += 4)
	write_dtag(ptr + i, dtag);
}
