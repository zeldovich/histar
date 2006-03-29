#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <bits/unimpl.h>

// Some BSD gunk

// Prototype to make GCC happy
uint32_t arc4random(void);
void arc4random_stir(void);
int ttyslot(void);

uint32_t
arc4random(void)
{
    return rand();
}

void
arc4random_stir(void)
{
    ;    
}

void
sync(void)
{
    set_enosys();
}

int 
ttyslot(void)
{
    return 0;    
}
