#ifndef LINUX_ARCH_LIND_LINUX_STRING_H
#define LINUX_ARCH_LIND_LINUX_STRING_H

#define __HAVE_ARCH_MEMCPY	1
#define __HAVE_ARCH_MEMMOVE	1
#define __HAVE_ARCH_MEMSET	1
#define __HAVE_ARCH_MEMCMP	1
#define __HAVE_ARCH_MEMCHR	1

#define __HAVE_ARCH_STRLEN	1
#define __HAVE_ARCH_STRNLEN	1
#define __HAVE_ARCH_STRCPY	1
#define __HAVE_ARCH_STRCAT	1
#define __HAVE_ARCH_STRCMP	1

void *memcpy(void *to, const void *from, size_t len); 
void *memset(void *s, int c, size_t n);
void *memmove(void * dest,const void *src,size_t count);
int memcmp(const void * cs,const void * ct,size_t count);
void *memchr(const void *s, int c, size_t n);

size_t strlen(const char * s);
size_t strnlen(const char *s, size_t maxlen);
char *strcpy(char * dest,const char *src);
char *strcat(char * dest, const char * src);
int strcmp(const char * cs,const char * ct);

#endif
