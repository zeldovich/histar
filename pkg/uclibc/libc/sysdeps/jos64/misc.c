#include <inc/syscall.h>

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
    int64_t ts = sys_pstate_timestamp();
    if (ts < 0) {
	cprintf("sync: sys_pstate_timestamp: %s\n", e2s(ts));
	return;
    }

    int r = sys_pstate_sync(ts);
    if (r < 0)
	cprintf("sync: sys_pstate_sync: %s\n", e2s(r));
}

int 
ttyslot(void)
{
    return 0;    
}
