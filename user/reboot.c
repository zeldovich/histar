#include <stdio.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

int 
main (int ac, char **av) 
{
    int r = sys_machine_reboot();
    if (r < 0)
	printf("reboot: %s\n", e2s(r));
    else
	printf("reboot: done\n");

    return r;
}
