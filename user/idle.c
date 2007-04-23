#include <inc/syscall.h>
#include <machine/trapcodes.h>

int
main(int ac, char **av)
{
    for (;;)
	sys_self_yield();
}
