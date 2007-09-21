#include <stdio.h>
#include <machine/um.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <kern/handle.h>
#include <inc/error.h>

int
main(int ac, char **av)
{
    printf("HiStar/um: starting..\n");

    uint32_t mem_bytes = 4096 * 4096;

    um_cons_init();
    um_mem_init(mem_bytes);
    um_time_init();

    kobject_init();
    sched_init();
    pstate_init();
    prof_init();
    key_generate();

    cprintf("Ready.\n");

    um_bench();
}
