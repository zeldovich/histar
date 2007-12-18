#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <inc/fs.h>

libc_hidden_proto(gethostname)
libc_hidden_proto(uname)

int
gethostname(char *name, size_t len)
{
    struct fs_inode f;
    int r = fs_namei("/etc/hostname", &f);
    if (r < 0) {
	__set_errno(EIO);
	return -1;
    }

    ssize_t cc = fs_pread(f, name, len - 1, 0);
    if (cc < 0) {
	__set_errno(EIO);
	return -1;
    }

    name[cc] = '\0';
    return 0;
}

int
sethostname(const char *name, size_t len)
{
    struct fs_inode f;
    int r = fs_namei("/etc/hostname", &f);
    if (r < 0) {
	__set_errno(EPERM);
	return -1;
    }

    ssize_t cc = fs_pwrite(f, name, len, 0);
    if (cc < 0) {
	__set_errno(EPERM);
	return -1;
    }

    fs_resize(f, len);
    return 0;
}

#define stringify(x) #x
#define stringify_eval(x) stringify(x)

int
uname (struct utsname *name)
{
    static char hostname[128];
    gethostname(&hostname[0], sizeof(hostname));

    sprintf(&name->sysname[0],  "Linux");
    sprintf(&name->nodename[0], "%s", &hostname[0]);
    sprintf(&name->release[0],  "HiStar");
    sprintf(&name->version[0],  "%s", JOS_BUILD_VERSION);
    sprintf(&name->machine[0],  stringify_eval(__UCLIBC_ARCH__));

    return 0;
}

libc_hidden_def(gethostname)
libc_hidden_def(uname)

