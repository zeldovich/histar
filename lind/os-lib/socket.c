#include <inc/assert.h>

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <os-lib/socket.h>
#include <linuxsyscall.h>

void
linux_make_async(int fd)
{
    int r;
    if ((r = linux_fcntl(fd, F_GETFL, 0)) < 0 ||
	(r = linux_fcntl(fd, F_SETFL, r | O_NONBLOCK) < 0))
	panic("linux_make_async: failed %d", r);
    return;
}
