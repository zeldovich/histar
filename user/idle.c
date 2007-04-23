#include <inc/syscall.h>
#include <machine/trapcodes.h>

int __attribute__((noreturn))
main(int ac, char **av)
{
    for (;;)
	sys_self_yield();
}
