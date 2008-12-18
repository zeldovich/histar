#include <stdio.h>
#include <machine/um.h>
#include <machine/nacl.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <kern/handle.h>
#include <kern/uinit.h>
#include <kern/embedbin.h>
#include <inc/error.h>

int
main(int ac, char **av)
{
    if (ac < 3) {
	printf("usage: %s bootstrap.nexe memfile\n", av[0]);
	return -1;
    }

    um_cons_init();
    nacl_mem_init(av[2], av[1]);
    nacl_trap_init();
    nacl_timer_init();
    
    kobject_init();
    sched_init();
    pstate_init();
    prof_init();
    user_init();

    cprintf("=== kernel ready, calling thread_run() ===\n");
    thread_run();
}
