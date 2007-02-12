extern "C" {
#include <stdio.h>
}

#include <dj/dis.hh>
#include <dj/djgatesrv.hh>

int
main(int ac, char **av)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djfsd";

    djgatesrv dgs(&gd, wrap(dj_posixfs_service));

    cobj_ref g = dgs.gate();
    printf("djfsd: gate %lu.%lu\n", g.container, g.object);

    for (;;)
	sleep(60);
}
