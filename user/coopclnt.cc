extern "C" {
#include <inc/syscall.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
}

#include <inc/cooperate.hh>

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s container gate-id\n", av[0]);
	return -1;
    }

    cobj_ref coop_gate = COBJ(atoi(av[1]), atoi(av[2]));

    coop_sysarg arg_values[8];
    memset(&arg_values[0], 0, sizeof(arg_values));

    arg_values[0].u.i = SYS_segment_create;
    arg_values[1].u.i = start_env->shared_container;
    arg_values[2].u.i = 1234;
    arg_values[3].u.i = 0;
    arg_values[4].u.i = 0;

    coop_gate_invoke(coop_gate, 0, 0, 0, arg_values);

    printf("coop gate invoked.\n");
}
