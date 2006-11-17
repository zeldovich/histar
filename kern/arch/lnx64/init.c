#include <machine/lnxdisk.h>
#include <machine/lnxpage.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/uinit.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char boot_cmdline[1024];

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s disk-file kernel-args\n", av[0]);
	exit(-1);
    }

    const char *disk_pn = av[1];
    snprintf(&boot_cmdline[0], sizeof(boot_cmdline), "%s", av[2]);

    printf("HiStar/lnx64: disk=%s\n", disk_pn);
    lnxdisk_init(disk_pn);

    timer_init();
    lnxpage_init();

    kobject_init();
    sched_init();
    pstate_init();
    prof_init();
    user_init();

    printf("Hmm, this probably isn't going to work...\n");

    schedule();
    thread_run(cur_thread);
}
