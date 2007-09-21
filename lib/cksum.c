#include <machine/types.h>
#include <inc/cksum.h>

uint64_t
cksum(uint64_t init, const void *vbuf, uint64_t count)
{
    const uint8_t *buf = vbuf;
    for (uint64_t i = 0; i < count; i++)
	init ^= (init << 5) ^ buf[i];
    return init;
}
