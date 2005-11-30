#ifndef JOS_INC_STRING_H
#define JOS_INC_STRING_H

#include <inc/types.h>

size_t	strlen(const char *s);
char *	strcpy(char *dest, const char *src);
char *  strncpy(char *dest, const char *src, size_t size);
size_t	strlcpy(char *dest, const char *src, size_t size);
int	strcmp(const char *s1, const char *s2);
int	strncmp(const char *s1, const char *s2, size_t size);
char *	strchr(const char *s, int c);
char *	strfind(const char *s, char c);
char *  strcat(char *dest, const char *src);

void *	memset(void *dest, int c, size_t len);
void *	memcpy(void *dest, const void *src, size_t len);
void *	memmove(void *dest, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);
void *	memfind(const void *s, int c, size_t len);

long	strtol(const char *s, char **endptr, int base);
int	strtoull(const char *s, char **endptr, int base, uint64_t *result);
int	atoi(const char *nptr);

#endif /* not JOS_INC_STRING_H */
