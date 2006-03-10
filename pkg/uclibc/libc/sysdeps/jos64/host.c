#include <unistd.h>
#include <errno.h>
#include <string.h>

static char hostname[32];

int
gethostname(char *name, size_t len)
{
    if (len <= strlen(hostname)) {
	__set_errno(EINVAL);
	return -1;
    }

    strcpy(name, &hostname[0]);
    return 0;
}

int
sethostname(const char *name, size_t len)
{
    if (len >= sizeof(hostname)) {
	__set_errno(EINVAL);
	return -1;
    }

    strcpy(&hostname[0], name);
    return 0;
}
