#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

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

int
uname (struct utsname *name)
{
    name->sysname[0] = '\0';
    name->nodename[0] = '\0';
    name->release[0] = '\0';
    name->version[0] = '\0';
    name->machine[0] = '\0';

    return 0;
}
