#include <machine/lnxdisk.h>
#include <machine/lnxpage.h>
#include <machine/lnxinit.h>
#include <machine/actor.h>
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

void
lnx64_init(const char *disk_pn, const char *cmdline, uint64_t membytes)
{
    snprintf(&boot_cmdline[0], sizeof(boot_cmdline), "%s", cmdline);

    printf("HiStar/lnx64: disk=%s, membytes=%ld, cmdline=%s\n",
	   disk_pn, membytes, cmdline);

    lnxdisk_init(disk_pn);
    lnxpage_init(membytes);
    timer_init();

    kobject_init();
    sched_init();
    pstate_init();
    prof_init();

    actor_init();
}
