#include <inc/syscall.h>
#include <malloc.h>
#include <stdlib.h>

int
main(int ac, char **av)
{
    const size_t size = 2 * 1024 * 1024;
    char *d = malloc(size);
    volatile char *p = d;
    int64_t r;
    for (;;) {
	r = random() % size;
	p = d + r;
	*p;
    }
}
