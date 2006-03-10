#include <stdint.h>
#include <stdlib.h>

// Some BSD gunk
uint32_t
arc4random(void)
{
    return rand();
}
