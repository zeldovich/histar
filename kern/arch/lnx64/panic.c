#include <stdlib.h>

#include <kern/arch.h>
#include <kern/lib.h>

static const char *panicstr;

void
_panic(const char *file, int line, const char *fmt, ...)
{
    if (panicstr)
	exit(-1);
    panicstr = fmt;

    va_list ap;
    va_start(ap, fmt);
    printf("[%"PRIu64"] kpanic: %s:%d: ",
	    cur_thread ? cur_thread->th_ko.ko_id : 0,
	    file, line);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);

    exit(-1);
}
