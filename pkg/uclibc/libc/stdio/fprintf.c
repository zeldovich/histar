/* Copyright (C) 2004       Manuel Novoa III    <mjn3@codepoet.org>
 *
 * GNU Library General Public License (LGPL) version 2 or later.
 *
 * Dedicated to Toni.  See uClibc/DEDICATION.mjn3 for details.
 */

#include "_stdio.h"
#include <stdarg.h>

libc_hidden_proto(vfprintf)

libc_hidden_proto(fprintf)
int fprintf(FILE * __restrict stream, const char * __restrict format, ...)
{
	va_list arg;
	int rv;

	va_start(arg, format);
	rv = vfprintf(stream, format, arg);
	va_end(arg);

	return rv;
}
libc_hidden_def(fprintf)

/*
 * gcc 4.3.2's libgcc references __fprintf_chk
 */

int __fprintf_chk(FILE *stream, int flags, const char *format, ...);

libc_hidden_proto(__fprintf_chk)
int
__fprintf_chk(FILE *stream, int flags, const char *format, ...)
{
    va_list arg;
    int rv;

    va_start(arg, format);
    rv = vfprintf(stream, format, arg);
    va_end(arg);

    return rv;
}
libc_hidden_def(__fprintf_chk)

