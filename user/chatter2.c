#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    for (;;) {
	cprintf("foo and yielding.\n");
	for (int i = 0; i < 10000; i++)
	    sys_thread_yield();
    }
}
