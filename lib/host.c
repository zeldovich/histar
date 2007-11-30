#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>

static char hostname[32] = "histar-box";

libc_hidden_proto(gethostname)
libc_hidden_proto(uname)

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

    strncpy(&hostname[0], name, sizeof(hostname) - 1);
    return 0;
}

#define stringify(x) #x
#define stringify_eval(x) stringify(x)

int
uname (struct utsname *name)
{
    sprintf(&name->sysname[0],  "Linux");
    sprintf(&name->nodename[0], "%s", &hostname[0]);
    sprintf(&name->release[0],  "HiStar");
    sprintf(&name->version[0],  "%s", JOS_BUILD_VERSION);
    sprintf(&name->machine[0],  stringify_eval(__UCLIBC_ARCH__));

    return 0;
}

libc_hidden_def(gethostname)
libc_hidden_def(uname)

