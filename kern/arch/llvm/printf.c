#include <kern/lib.h>
#include <stdio.h>

int
cprintf(const char *fmt, ...)
{
    va_list ap;
    int cnt;

    va_start(ap, fmt);
    cnt = vprintf(fmt, ap);
    va_end(ap);

    return cnt;
}

int
vcprintf(const char *fmt, va_list ap)
{
    return vprintf(fmt, ap);
}
