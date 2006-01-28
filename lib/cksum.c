#include <machine/types.h>

uint64_t
cksum(uint64_t init, uint8_t *buf, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
	init ^= (init << 5) ^ buf[i];
    return init;
}
