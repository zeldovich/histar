#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    for (;;)
	cprintf("foo foo foo.\n");
}
