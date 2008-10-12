#include <machine/mp.h>
#include <kern/arch.h>
#include <kern/lib.h>

static uint8_t
sum(uint8_t * a, uint32_t length)
{
    uint8_t s = 0;
    for (uint32_t i = 0; i < length; i++)
	s += a[i];
    return s;
}

static struct mp_fptr *
mp_search1(physaddr_t pa, int len)
{
    uint8_t *start = (uint8_t *) pa2kva(pa);
    for (uint8_t * p = start; p < (start + len); p += sizeof(struct mp_fptr)) {
	if ((memcmp(p, "_MP_", 4) == 0)
	    && (sum(p, sizeof(struct mp_fptr)) == 0))
	    return (struct mp_fptr *) p;
    }
    return 0;
}

struct mp_fptr *
mp_search(void)
{
    struct mp_fptr *ret;
    uint8_t *bda;
    physaddr_t pa;

    bda = (uint8_t *) pa2kva(0x400);
    if ((pa = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
	if ((ret = mp_search1(pa, 1024)))
	    return ret;
    } else {
	pa = ((bda[0x14] << 8) | bda[0x13]) * 1024;
	if ((ret = mp_search1(pa - 1024, 1024)))
	    return ret;
    }
    return mp_search1(0xF0000, 0x10000);
}
