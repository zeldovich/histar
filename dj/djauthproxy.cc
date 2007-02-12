extern "C" {
#include <stdio.h>
}

#include <dj/dis.hh>
#include <dj/djgatesrv.hh>
#include <inc/authclnt.hh>

bool
dj_auth_proxy(const djcall_args &in, djcall_args *out)
{
    //djauth_request 
    return false;
}

int
main(int ac, char **av)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djauthproxy";

    djgatesrv dgs(&gd, wrap(dj_auth_proxy));

    cobj_ref g = dgs.gate();
    printf("djauthproxy: gate %lu.%lu\n", g.container, g.object);

    for (;;)
	sleep(60);
}
