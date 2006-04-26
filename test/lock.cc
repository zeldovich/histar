#include <test/josenv.hh>

extern "C" {
#include <machine/stackwrap.h>
}

void
lock_acquire(struct lock *l)
{
}

int
lock_try_acquire(struct lock *l)
{
    return 0;
}

void
lock_release(struct lock *l)
{
}

void
lock_init(struct lock *l)
{
}
