#include <inc/assert.h>
#include <inc/lib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void
_panic(const char *file, int line, const char *fmt, ...)
{
	extern char *__progname;
	va_list ap;

	va_start(ap, fmt);

	// Print the panic message
	fprintf(stderr, "%s: user panic at %s:%d: ",
		__progname ? : "unknown", file, line);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	print_backtrace();

	exit(-1);
}
