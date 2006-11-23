#include <machine/lnxinit.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s disk-file kernel-args\n", av[0]);
	exit(-1);
    }

    const char *disk_pn = av[1];
    const char *cmdline = av[2];

    lnx64_init(disk_pn, cmdline, 64 * 1024 * 1024);

    user_init();
    printf("Hmm, this probably isn't going to work...\n");
    schedule();
    thread_run(cur_thread);
}
