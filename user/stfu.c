#include <inc/syscall.h>
#include <stdio.h>

int
main()
{
	printf("Toggling reserve output...\n");
	sys_toggle_debug(1);
	return 0;
}
