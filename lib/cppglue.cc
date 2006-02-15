extern "C" {
#include <inc/lib.h>
}

void *
operator new(size_t size)
{
    void *b = malloc(size);
    if (b == 0)
	panic("out of memory, but cannot throw an exception");
    return b;
}

void
operator delete(void *b)
{
    free(b);
}
