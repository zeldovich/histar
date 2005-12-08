#ifndef JOS_KERN_LIB_H
#define JOS_KERN_LIB_H

#include <inc/stdarg.h>
#include <inc/types.h>

void *memset(void *dest, int c, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
char *strchr (const char *p, int ch);
int strcmp (const char *s1, const char *s2);
size_t strlen (const char *);

void vprintfmt (void (*putch) (int, void *), void *putdat,
		const char *fmt, va_list ap)
		__attribute__((__format__ (__printf__, 3, 0)));
int vcprintf (const char *fmt, va_list ap)
	__attribute__((__format__ (__printf__, 1, 0)));
int cprintf (const char *fmt, ...)
	__attribute__((__format__ (__printf__, 1, 2)));

const char *e2s(int err);

void abort (void) __attribute__((__noreturn__));
void _panic (const char *file, int line, const char *fmt, ...)
	__attribute__((__format__ (__printf__, 3, 4)))
	__attribute__((__noreturn__));
#define panic(fmt, varargs...) _panic(__FILE__, __LINE__, fmt, ##varargs)

#define __stringify(s) #s
#define stringify(s) __stringify(s)
#define __FL__ __FILE__ ":" stringify (__LINE__)

#define assert(x)				\
do {						\
  if (!(x))					\
    panic("assertion failed at %s:%d:\n%s",	\
	  __FILE__, __LINE__, #x);		\
} while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#endif /* !JOS_KERN_LIB_H */
