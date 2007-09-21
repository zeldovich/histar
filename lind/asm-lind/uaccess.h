#ifndef __X86_64_UACCESS_H
#define __X86_64_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/prefetch.h>
#include <asm/page.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFFFFFFFFFFUL)
#define USER_DS		MAKE_MM_SEG(PAGE_OFFSET)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define access_ok(type,addr,size)	_access_ok((unsigned long)(addr),(size))

#include <archcall.h>

static inline int _access_ok(unsigned long addr, unsigned long size)
{
    /* XXX could do something like:
     * (((addr >= MEMORY_START) && (addr + size < MEMORY_END)) ||
     * ((addr >= _sdata) && (addr + size <_end)))
     */
    return 1;
}

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	unsigned long insn, fixup;
};

/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the ugliness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 */

#define get_user(x, ptr)					\
({								\
    int __gu_err = 0;						\
    typeof(x) __gu_val = 0;					\
    switch (sizeof(*(ptr))) {					\
    case 1:							\
    case 2:							\
    case 4:							\
    case 8:							\
	memcpy((void *) &__gu_val, ptr, sizeof (*(ptr)));	\
	break;							\
    default:							\
	__gu_val = 0;						\
	__gu_err = -EFAULT;				        \
	break;							\
    }								\
    (x) = (typeof(*(ptr))) __gu_val;				\
    __gu_err;							\
})
#define __get_user(x, ptr) get_user(x, ptr)

#define put_user(x, ptr)				\
({							\
    int __pu_err = 0;					\
    typeof(*(ptr)) __pu_val = (x);			\
    switch (sizeof (*(ptr))) {				\
    case 1:						\
    case 2:						\
    case 4:						\
    case 8:						\
	memcpy(ptr, &__pu_val, sizeof (*(ptr)));        \
	break;						\
    default:						\
	__pu_err = -EFAULT;			        \
	break;						\
    }							\
    __pu_err;						\
})
#define __put_user(x, ptr) put_user(x, ptr)

/*
 * Copy To/From Userspace
 */

#define copy_from_user(to, from, n)		(memcpy(to, from, n), 0)
#define copy_to_user(to, from, n)		(memcpy(to, from, n), 0)

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)
#define __copy_to_user(to, from, n) copy_to_user(to, from, n)
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

#define copy_to_user_ret(to,from,n,retval) ({ if (copy_to_user(to,from,n)) return retval; })

#define copy_from_user_ret(to,from,n,retval) ({ if (copy_from_user(to,from,n)) return retval; })


/*
 * Copy a null terminated string from userspace.
 */

static inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	char *tmp;
	strncpy(dst, src, count);
	for (tmp = dst; *tmp && count > 0; tmp++, count--)
		;
	return(tmp - dst); /* DAVIDM should we count a NUL ?  check getname */
}

/*
 * Return the size of a string (including the ending 0)
 *
 * Return 0 on exception, a value greater than N if too long
 */
static inline long strnlen_user(const char *src, long n)
{
	return(strlen(src) + 1); /* DAVIDM make safer */
}

#define strlen_user(str) strnlen_user(str, 32767)

/*
 * Zero Userspace
 */

static inline unsigned long
clear_user(void *to, unsigned long n)
{
	memset(to, 0, n);
	return 0;
}

#endif /* __X86_64_UACCESS_H */
