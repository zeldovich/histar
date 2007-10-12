/*
 * A very simple, and not very efficient, strlen implementation.
 */

#include <inc/types.h>

#ifdef JOS_KERNEL
#include <kern/lib.h>
#endif

size_t
strlen(const char *str)
{
    size_t len = 0;

    while (*str) {
	len++;
	str++;
    }

    return len;
}
