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

    uint64_t container = atoi(av[1]);
    uint64_t gate_id = atoi(av[2]);
    cobj_ref coop_gate = COBJ(container, gate_id);

    label segl(1);

    coop_sysarg arg_values[8];
    memset(&arg_values[0], 0, sizeof(arg_values));

    arg_values[0].u.i = SYS_segment_create;
    arg_values[1].u.i = container;
    arg_values[2].u.i = 1234;

    arg_values[3].u.l = &segl;
    arg_values[3].is_label = 1;

    arg_values[4].u.i = 0;

    coop_gate_invoke(coop_gate, 0, 0, 0, arg_values);

    printf("coop gate invoked.\n");
}
