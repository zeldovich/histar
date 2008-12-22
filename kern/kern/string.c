#include <kern/lib.h>
#include <inc/error.h>

char *
strncpy(char *dst, const char *src, size_t n) {
	size_t i;
	char *p = dst;

	for (i = 0; i < n; i++) {
		*dst = *src;
		dst++;
		if (*src) {
			src++; // This does null padding if length of src is less than n
		}
	}

	return p;
}

int
strncmp(const char *p, const char *q, size_t n)
{
	while (n > 0 && *p && *p == *q)
		n--, p++, q++;
	if (n == 0)
		return 0;
	else
		return (int) ((unsigned char) *p - (unsigned char) *q);
}

int
strcmp(const char *p, const char *q)
{
	while (*p && *p == *q)
		p++, q++;
	return (int) ((unsigned char) *p - (unsigned char) *q);
}

char *
strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == c)
			return (char *) s;
	return 0;
}

char *
strstr(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);

    for (; *haystack; haystack++)
	if (!strncmp(haystack, needle, nlen))
	    return (char *) haystack;

    return 0;
}
