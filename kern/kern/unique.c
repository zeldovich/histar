#include <kern/unique.h>

uint64_t
unique_alloc()
{
    static uint64_t counter;

    return counter++;
}
