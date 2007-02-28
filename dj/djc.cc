extern "C" {
#include <inc/syscall.h>
}

#include <inc/labelutil.hh>
#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/gatesender.hh>
#include <dj/djsrpc.hh>
#include <dj/internalx.h>
#include <dj/djautorpc.hh>

int
main(int ac, char **av)
{
    dj_global_cache djcache;

    if (ac != 4) {
	printf("Usage: %s host-pk call-ct gate-ct.id\n", av[0]);
	exit(-1);
    }

    gate_sender gs;

    str pkstr(av[1]);
    //ptr<sfspub> sfspub = sfscrypt.alloc(pkstr, SFS_VERIFY | SFS_ENCRYPT);
    //assert(sfspub);
    //dj_pubkey k = sfspub2dj(sfspub);
    dj_pubkey k = gs.hostkey();

    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = atoi(av[2]);
    ep.ep_gate->gate <<= av[3];

    dj_delegation_set dset;
    dj_catmap cm;
    dj_message m;

    /* Create a local container for call-related state */
    label localct_l(1);
    int64_t local_ct = sys_container_alloc(start_env->shared_container,
					   localct_l.to_ulabel(), "blah",
					   0, CT_QUOTA_INF);
    error_check(local_ct);

    /* Allocate a new category to taint with.. */
    uint64_t tcat = handle_alloc();
    warn << "allocated category " << tcat << " for tainting\n";

    dj_mapreq mapreq;
    mapreq.ct = local_ct;
    mapreq.lcat = tcat;
    mapreq.gcat.integrity = 0;

    label xgrant(3);
    xgrant.set(tcat, LB_LEVEL_STAR);

    dj_message_endpoint map_ep;
    map_ep.set_type(EP_MAPCREATE);

    dj_autorpc local_ar(&gs, 1, gs.hostkey(), djcache);

    dj_cat_mapping local_cme;
    dj_delivery_code c = local_ar.call(map_ep, mapreq, local_cme,
				       0, 0, 0, &xgrant);
    if (c != DELIVERY_DONE)
	warn << "error talking to mapcreate: code " << c << "\n";

    warn << "Local dj_cat_mapping: "
	 << local_cme.gcat << ", "
	 << local_cme.lcat << ", "
	 << local_cme.user_ct << ", "
	 << local_cme.res_ct << ", "
	 << local_cme.res_gt << "\n";
    djcache[gs.hostkey()]->cmi_.insert(local_cme);

    mapreq.ct = ep.ep_gate->msg_ct;
    mapreq.gcat = local_cme.gcat;
    mapreq.lcat = 0;

    dj_autorpc remote_ar(&gs, 1, k, djcache);
    dj_cat_mapping remote_cme;
    c = remote_ar.call(map_ep, mapreq, remote_cme, 0, &xgrant);
    if (c != DELIVERY_DONE)
	warn << "error from remote mapcreate: code " << c << "\n";

    warn << "Remote dj_cat_mapping: "
	 << remote_cme.gcat << ", "
	 << remote_cme.lcat << ", "
	 << remote_cme.user_ct << ", "
	 << remote_cme.res_ct << ", "
	 << remote_cme.res_gt << "\n";
    djcache[k]->cmi_.insert(remote_cme);

    /* Send a real echo request now.. */
    dj_gatename arg;
    dj_gatename res;
    arg.gate_ct = 123456;
    arg.gate_id = 654321;
    res.gate_ct = 101;
    res.gate_id = 102;
    c = remote_ar.call(ep, arg, res);
    warn << "echo response code = " << c << "\n";
    if (c == DELIVERY_DONE)
	warn << "autorpc echo: " << res.gate_ct << "." << res.gate_id << "\n";
}
