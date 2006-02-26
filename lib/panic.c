#include <inc/stdarg.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>

void
_panic(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	// Print the panic message
	cprintf("user panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");

	print_backtrace();

	exit(-1);
}
