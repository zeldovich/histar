extern "C" {
#include <inc/syscall.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/cooperate.hh>

int
main(int ac, char **av)
{
    label l(1);
    l.set(start_env->process_grant, LB_LEVEL_STAR);
    l.set(start_env->process_taint, LB_LEVEL_STAR);

    label clear(2);
    label verify(3);

    label segl(1);

    coop_sysarg arg_values[8];
    char arg_freemask[8];

    memset(&arg_values[0], 0, sizeof(arg_values));
    memset(&arg_freemask[0], 0, sizeof(arg_freemask));

    arg_values[0].u.i = SYS_segment_create;
    arg_values[1].u.i = start_env->shared_container;
    arg_values[2].u.i = 1234;

    arg_values[3].u.l = &segl;
    arg_values[3].is_label = 1;

    arg_values[4].u.i = 0;

    cobj_ref g =
	coop_gate_create(start_env->shared_container,
			 &l, &clear, &verify,
			 arg_values, arg_freemask);

    printf("coop gate created: %ld %ld\n", g.container, g.object);
    sys_self_halt();
}
