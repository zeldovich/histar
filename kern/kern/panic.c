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

#define PRINT_STACK(i) \
    cprintf("Call stack[%d]: %p\n", i, __builtin_return_address(i))

    PRINT_STACK(0);
    PRINT_STACK(1);
    PRINT_STACK(2);
    PRINT_STACK(3);
    PRINT_STACK(4);
    PRINT_STACK(5);
    PRINT_STACK(6);
    PRINT_STACK(7);
    

 dead:
    abort();
}
