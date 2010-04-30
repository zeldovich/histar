#include <inc/syscall.h>
#include <unistd.h>

int
main(int ac, char **av)
{
    for (;;)
	sleep(120);
}
