#ifndef _MISC_H_
#define _MISC_H_

#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

extern "C" {
#include <inc/stdio.h>
}

#if 1
#define LOGD(fmt,args...)	cprintf(fmt, ##args)
#define LOGE(fmt,args...)	cprintf(fmt, ##args)
#define LOGI(fmt,args...)	cprintf(fmt, ##args)
#define LOGW(fmt,args...)	cprintf(fmt, ##args)
#define LOGV(fmt,args...)	cprintf(fmt, ##args)
#else
#define LOGD(fmt,args...)
#define LOGE(fmt,args...)
#define LOGI(fmt,args...)
#define LOGW(fmt,args...)
#define LOGV(fmt,args...)
#endif

typedef int status_t;

#define NO_ERROR	 0
#define NOT_ENOUGH_DATA	-1
#define BAD_VALUE	-2
#define NO_MEMORY	-3

#define OS_PATH_SEPARATOR	'/'

extern "C" int pthread_cond_timeout_np(pthread_cond_t *, pthread_mutex_t *, unsigned);

static inline uint32_t
be32_to_cpu(uint32_t x)
{
	return (((x & 0xff000000) >> 24) |
		((x & 0x00ff0000) >>  8) |
		((x & 0x0000ff00) <<  8) |
		((x & 0x000000ff) << 24));
}

static inline uint32_t
cpu_to_be32(uint32_t x)
{
	return (be32_to_cpu(x));
}

#define ERR_PTR(_x)	((void *)(intptr_t)(_x))
#define PTR_ERR(_x)	((int)(intptr_t)(_x))	
#define IS_ERR(_x)	((intptr_t)(_x) < 0)
#define BUG_ON(_x)	assert(!(_x))

#define MKDEV(_x, _y)	(((_x) << 16) | (_y))
#define MAJOR(_x)	(((_x) >> 16) & 0xffff)
#define MINOR(_x)	((_x) & 0xffff)

#endif /* !_MISC_H_ */
