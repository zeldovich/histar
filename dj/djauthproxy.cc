extern "C" {
#include <stdio.h>
}

#include <async.h>
#include <crypt.h>
#include <inc/authclnt.hh>

#include <dj/dis.hh>
#include <dj/djgatesrv.hh>
#include <dj/djauth.h>

bool
dj_auth_proxy(const djcall_args &in, djcall_args *out)
{
    djauth_request req;
    djauth_reply res;

    if (!str2xdr(req, in.data)) {
	warn << "dj_auth_proxy: cannot unmarshal\n";
	return false;
    }

    try {
	uint64_t ug, ut;
	auth_login(req.username, req.password, &ug, &ut);
	out->grant.set(ug, LB_LEVEL_STAR);
	out->grant.set(ut, LB_LEVEL_STAR);
	res.ok = true;
    } catch (std::exception &e) {
	warn << "dj_auth_proxy: " << e.what() << "\n";
	res.ok = false;
    }

    out->data = xdr2str(res);
    return true;
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
