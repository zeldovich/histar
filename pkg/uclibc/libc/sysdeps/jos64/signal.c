#include <errno.h>
#include <signal.h>

int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    __set_errno(ENOSYS);
    return -1;
}
