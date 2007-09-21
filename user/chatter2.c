#include <inc/lib.h>
#include <stdio.h>

int
main(int ac, char **av)
{
    uint64_t a, b, c;

    for (;;) {
	for (int i = 0; i < 100000000; i++)
	    a = b + c;

	printf("foo %ld\n", thread_id());
    }
}
