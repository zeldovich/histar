#include <machine/types.h>
#include <kern/ht.h>

uint64_t
ht_hash(uint8_t *blob, uint32_t size)
{
    uint64_t hv = 0;
    for (uint32_t i = 0; i < size; i++)
	hv ^= (hv << 8) ^ blob[i];
    return hv;
}
