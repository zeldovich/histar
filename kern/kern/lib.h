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
		const char *fmt, va_list ap);
int vcprintf (const char *fmt, va_list ap);
int cprintf (const char *fmt, ...);

void abort (void);
void _panic (const char *file, int line, const char *fmt, ...);
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

#define __stringify(s) #s
#define stringify(s) __stringify(s)
#define __FL__ __FILE__ ":" stringify (__LINE__)

#define assert(x)				\
do {						\
  if (!(x))					\
    panic("assertion failed at %s:%d:\n%s",	\
	  __FILE__, __LINE__, #x);		\
} while (0)

#endif /* !JOS_KERN_LIB_H */
