#include <kern/lib.h>
#include <kern/thread.h>

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
static const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    if (panicstr)
	goto dead;
    panicstr = fmt;

    va_start(ap, fmt);
    cprintf("[%"PRIu64"] kpanic: %s:%d: ",
	    cur_thread ? cur_thread->th_ko.ko_id : 0,
	    file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);

 dead:
    abort();
}
