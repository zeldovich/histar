#include <test/josenv.hh>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <kern/lib.h>
}

#include <inc/error.hh>

void
_panic(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    char buf[4096];
    vsprintf(&buf[0], fmt, ap);
    va_end(ap);

    throw basic_exception("kernel panic: %s:%d: %s", file, line, &buf[0]);
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
