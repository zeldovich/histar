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
    if (ac != 6) {
	printf("Usage: %s proxy-host proxy-ct proxy-gate username password\n", av[0]);
	exit(-1);
    }

    label pub(1);
    int64_t ctid = sys_container_alloc(start_env->shared_container,
				       pub.to_ulabel(), "foobar", 0, CT_QUOTA_INF);
    error_check(ctid);

    gate_sender gs;

    str pkstr(av[1]);
    dj_pubkey pk;
    if (pkstr == "0") {
	pk = gs.hostkey();
    } else {
	ptr<sfspub> sfspub = sfscrypt.alloc(pkstr, SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	pk = sfspub2dj(sfspub);
    }

    dj_slot ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = strtoll(av[2], 0, 0);
    ep.ep_gate->gate <<= av[3];

    dj_global_cache cache;

    authproxy_arg arg;
    authproxy_res res;

    arg.username = av[4];
    arg.password = av[5];
    arg.map_ct = ep.ep_gate->msg_ct;
    arg.return_map_ct = ctid;

    label tl_before;
    thread_cur_label(&tl_before);

    dj_autorpc remote_ar(&gs, 5, pk, cache);

    dj_delivery_code c = remote_ar.call(ep, arg, res);
    if (c != DELIVERY_DONE)
	fatal << "rpc call: code " << c << "\n";
    if (!res.ok)
	fatal << "authproxy: not ok\n";

    warn << "all done\n";
    warn << res.resok->ut_remote.gcat << "\n";
    warn << res.resok->ug_remote.gcat << "\n";

    label tl_after;
    thread_cur_label(&tl_after);

    warn << "label before: " << tl_before.to_string() << "\n";
    warn << "label after:  " << tl_after.to_string() << "\n";
}
