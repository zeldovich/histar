#include <sys/sysinfo.h>
#include <bits/unimpl.h>

int 
sysinfo(struct sysinfo *info) 
{
    set_enosys();
    return -1;
}
