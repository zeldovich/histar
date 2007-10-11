#include <string.h>
#include <sys/sysinfo.h>
#include <inc/syscall.h>
#include <inc/lib.h>

int 
sysinfo(struct sysinfo *info) 
{
    memset(info, 0, sizeof(*info));
    info->uptime = sys_clock_nsec() / NSEC_PER_SECOND;
    return 0;
}
