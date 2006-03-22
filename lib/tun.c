#include <inc/tun.h>
#include <inc/fd.h>

#include <errno.h>

int
tun_open(struct fs_inode tseg, const char *pn_suffix)
{
    errno = EINVAL;
    return -1;
}
