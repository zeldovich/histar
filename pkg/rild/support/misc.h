#ifndef _MISC_H_
#define _MISC_H_

#if 1
#define LOGD(fmt,args...)	fprintf(stderr, fmt, ##args)
#define LOGE(fmt,args...)	fprintf(stderr, fmt, ##args)
#define LOGI(fmt,args...)	fprintf(stderr, fmt, ##args)
#define LOGW(fmt,args...)	fprintf(stderr, fmt, ##args)
#define LOGV(fmt,args...)	fprintf(stderr, fmt, ##args)
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

#endif /* !_MISC_H_ */
