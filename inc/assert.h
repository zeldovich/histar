/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ASSERT_H
#define JOS_INC_ASSERT_H

#include <inc/stdio.h>

void _panic(const char*, int, const char*, ...)
	__attribute__((noreturn))
	__attribute__((__format__ (__printf__, 3, 4)));

#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)		\
	do { if (!(x)) panic("assertion failed: %s", #x); } while (0)


/* Force a compilation error if condition is false, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define static_assert_zero(e) (sizeof(char[1 - 2 * !(e)]) - 1)

/* Generate a compile-time error if 'e' is false. */
#define static_assert(e) ((void)sizeof(char[1 - 2 * !(e)]))

#endif /* !JOS_INC_ASSERT_H */
