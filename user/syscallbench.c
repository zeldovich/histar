#include <machine/x86.h>
#include <inc/syscall.h>
#include <stdio.h>

int
main(int ac, char **av)
{
    uint64_t msec0 = sys_clock_msec();
    uint64_t tsc0 = read_tsc();

    int rounds = 1000000;
    for (int i = 0; i < rounds; i++)
	sys_self_id();

    uint64_t msec1 = sys_clock_msec();
    uint64_t tsc1 = read_tsc();

    printf("Syscall latency: %ld msec / %ld cycles for %d rounds\n",
	   msec1 - msec0, tsc1 - tsc0, rounds);
    printf("Average cycles per syscall: %ld\n", (tsc1 - tsc0) / rounds);
}
