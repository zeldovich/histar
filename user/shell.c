#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>

int
main(int ac, char **av)
{
    cprintf("JOS shell\n");

    for (;;) {
	char *cmd = readline("jos> ");
	cprintf("cmd: %s\n", cmd);
    }
}
