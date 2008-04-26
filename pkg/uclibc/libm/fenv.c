#include <fenv.h>
#include <bits/unimpl.h>

int
fesetround(int rmode)
{
    set_enosys();
    return -1;
}
