extern "C" {
#include <stdio.h>
}

#include <dj/dis.hh>
#include <dj/djgatesrv.hh>

int
main(int ac, char **av)
{
    label tl, tc;
    thread_cur_label(&tl);
    thread_cur_clearance(&tc);

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djfsd";
    gd.label_ = &tl;
    gd.clearance_ = &tc;

    djgatesrv dgs(&gd, wrap(dj_posixfs_service));

    cobj_ref g = dgs.gate();
    printf("djfsd: gate %lu.%lu\n", g.container, g.object);
}
