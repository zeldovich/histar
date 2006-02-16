#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/lib.h>

static FILE stdf[] = { {0}, {1}, {2} };
FILE *stdin = &stdf[0];
FILE *stdout = &stdf[1];
FILE *stderr = &stdf[2];

int
fputs(const char *s, FILE *stream)
{
    return write(stream->fd, s, strlen(s));
}
