#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <inc/syscall.h>

int
main(int ac, char **av)
{
    int i;
    volatile int j;
    uint64_t before = sys_clock_nsec();
    for (i = 0; i < 1000 * 1000 * 1000; i++) {
	j = j ^ (j + i);
    }
    printf("nsec: %" PRIu64 "\n", sys_clock_nsec() - before);
    return i;
}
