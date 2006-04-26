#include <test/josenv.hh>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <kern/lib.h>
}

void
_panic(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf("kernel panic: %s:%d: ", file, line);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    exit(-1);
}

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
