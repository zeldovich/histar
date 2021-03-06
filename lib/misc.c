#include <inc/syscall.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>

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

#ifndef SHARED
int
dl_iterate_phdr(int (*callback) (struct dl_phdr_info *info,
				 size_t size, void *data),
		void *data)
{
    return 0;
}
#endif

#if defined(JOS_ARCH_i386) || defined(JOS_ARCH_amd64)
#include <sys/io.h>

int
iopl(int level)
{
    set_enosys();
    return -1;
}

int
ioperm(unsigned long from, unsigned long num, int turn_on)
{
    set_enosys();
    return -1;
}
#endif

void *__tls_get_addr(void) __attribute__((noreturn));

void *
__tls_get_addr(void)
{
    fprintf(stderr, "__tls_get_addr: not implemented\n");
    exit(-1);
}

strong_alias(__tls_get_addr, ___tls_get_addr)
