#ifndef JOS_INC_STDIO_H
#define JOS_INC_STDIO_H

#include <inc/types.h>
#include <inc/stdarg.h>

// lib/printfmt.c
void	printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...)
	    __attribute__((__format__ (__printf__, 3, 4)));
void	vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);

int	snprintf(char *str, size_t size, const char *fmt, ...)
	    __attribute__((__format__ (__printf__, 3, 4)));
int	vsnprintf(char *str, size_t size, const char *fmt, va_list);

// lib/printf.c
int	cprintf(const char *fmt, ...)
	    __attribute__((__format__ (__printf__, 1, 2)));
int	vcprintf(const char *fmt, va_list);

#endif
