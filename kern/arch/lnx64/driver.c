#include <machine/lnxinit.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern void __k1_lnx64_init();
extern void __k2_lnx64_init();

extern void __k1_user_init();
extern void __k2_user_init();

extern void __k1_schedule();
extern void __k2_schedule();

extern void __k1_thread_run();
extern void __k2_thread_run();

extern void *__k1_cur_thread;
extern void *__k2_cur_thread;

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s disk-file\n", av[0]);
	exit(-1);
    }

    const char *disk_pn = av[1];
    const char *cmdline = "pstate=discard";

    // Technically this is pretty bad, because they're sharing the disk.
    // But, we aren't using the disk for anything yet, so whatever..
    __k1_lnx64_init(disk_pn, cmdline, 64 * 1024 * 1024);
    __k2_lnx64_init(disk_pn, cmdline, 64 * 1024 * 1024);

    __k1_user_init();
    __k2_user_init();

    printf("Hmm, this probably isn't going to work...\n");
    __k1_schedule();
    __k2_schedule();

    __k1_thread_run(__k1_cur_thread);
}
