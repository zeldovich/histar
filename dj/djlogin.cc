extern "C" {
#include <inc/syscall.h>
}

#include <dj/gatesender.hh>
#include <dj/djops.hh>
#include <dj/djcache.hh>
#include <dj/djautorpc.hh>
#include <dj/miscx.h>

int
main(int ac, char **av)
{
    if (ac != 4) {
	printf("Usage: %s proxy-gate username password\n", av[0]);
	exit(-1);
    }

    label pub(1);
    int64_t ctid = sys_container_alloc(start_env->shared_container,
				       pub.to_ulabel(), "foobar", 0, CT_QUOTA_INF);
    error_check(ctid);

    gate_sender gs;
    dj_pubkey pk = gs.hostkey();

    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = ctid;
    ep.ep_gate->gate <<= av[1];

    dj_global_cache cache;

    authproxy_arg arg;
    authproxy_res res;

    arg.username = av[2];
    arg.password = av[3];
    arg.map_ct = ctid;
    arg.return_map_ct = ctid;

    dj_autorpc remote_ar(&gs, 5, pk, cache);

    dj_delivery_code c = remote_ar.call(ep, arg, res);
    if (c != DELIVERY_DONE)
	fatal << "rpc call: code " << c << "\n";
    if (!res.ok)
	fatal << "authproxy: not ok\n";

    warn << "all done\n";
    warn << res.resok->ut_remote.gcat << "\n";
    warn << res.resok->ug_remote.gcat << "\n";
}
