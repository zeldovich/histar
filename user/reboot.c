#include <inc/syscall.h>
#include <inc/stdio.h>

int 
main (int ac, char **av) 
{
    int r = sys_machine_reboot();
    cprintf("reboot: %s\n", e2s(r));
    return r;
}
