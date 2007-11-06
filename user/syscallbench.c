#include <inc/syscall.h>
#include <stdio.h>

#if defined(JOS_ARCH_amd64) || defined(JOS_ARCH_i386)
#include <machine/x86.h>

#define GET_TSC() read_tsc()
#endif

#if defined(JOS_ARCH_sparc)
#define GET_TSC() 0
#endif

int
main(int ac, char **av)
{
    uint64_t nsec0 = sys_clock_nsec();
    uint64_t tsc0 = GET_TSC();

    int rounds = 1000000;
    for (int i = 0; i < rounds; i++)
	sys_self_id();

    uint64_t nsec1 = sys_clock_nsec();
    uint64_t tsc1 = GET_TSC();

    printf("Syscall latency: %ld nsec / %ld cycles for %d rounds\n",
	   nsec1 - nsec0, tsc1 - tsc0, rounds);
    printf("Average cycles per syscall: %ld\n", (tsc1 - tsc0) / rounds);
}
