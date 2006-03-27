#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <bits/unimpl.h>

// Some BSD gunk

// Prototype to make GCC happy
uint32_t arc4random(void);

uint32_t
arc4random(void)
{
    return rand();
}

void
sync(void)
{
    set_enosys();
}
