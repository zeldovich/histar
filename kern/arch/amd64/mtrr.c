#include <machine/pmap.h>
#include <machine/x86.h>
#include <inc/error.h>

int
mtrr_set(physaddr_t base, uint64_t nbytes, uint32_t type)
{
    uint64_t new_base = base | type;
    uint64_t new_mask = (MTRR_MASK_FULL & ~(nbytes - 1)) | MTRR_MASK_VALID;

    uint32_t cnt = read_msr(MTRR_CAP) & MTRR_CAP_VCNT_MASK;
    for (uint32_t i = 0; i < cnt; i++) {
	uint64_t i_base = read_msr(MTRR_BASE(i));
	uint64_t i_mask = read_msr(MTRR_MASK(i));

	if (i_base == new_base && i_mask == new_mask) {
	    cprintf("mtrr_set: dup: base %lx, mask %lx\n", i_base, i_mask);
	    return 0;
	}

	if (i_mask & MTRR_MASK_VALID)
	    continue;

	write_msr(MTRR_BASE(i), new_base);
	write_msr(MTRR_MASK(i), new_mask);
	return 0;
    }

    return -E_NO_MEM;
}
