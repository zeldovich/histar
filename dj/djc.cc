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

    str pkstr(av[1]);
    ptr<sfspub> sfspub = sfscrypt.alloc(pkstr, SFS_VERIFY | SFS_ENCRYPT);
    assert(sfspub);
    dj_pubkey k = sfspub2dj(sfspub);

    uint64_t call_ct = atoi(av[2]);

    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    *ep.gate <<= av[3];

    gate_sender gs;

    dj_delegation_set dset;
    dj_catmap cm;
    dj_message m;

    /* Allocate a new category to taint with.. */
    uint64_t tcat = handle_alloc();
    warn << "allocated category " << tcat << " for tainting\n";

    /* XXX abuse of call_ct -- assumes same node */
    dj_mapreq mapreq;
    mapreq.ct = call_ct;
    mapreq.lcat = tcat;

    m.target.set_type(EP_MAPCREATE);
    m.token = 0;
    m.taint.deflevel = 1;
    m.glabel.deflevel = 3;
    m.gclear.deflevel = 0;

    label xgrant(3);
    xgrant.set(tcat, LB_LEVEL_STAR);

    dj_message replym;
    dj_delivery_code c = dj_rpc_call(&gs, k, 1, dset, cm, m,
				     xdr2str(mapreq), &replym, &xgrant);
    if (c != DELIVERY_DONE)
	warn << "error talking to mapcreate: code " << c << "\n";

    dj_cat_mapping tcatmap;
    if (!bytes2xdr(tcatmap, replym.msg))
	warn << "unmarshaling dj_cat_mapping\n";

    warn << "Got a dj_cat_mapping: "
	 << tcatmap.gcat << ", "
	 << tcatmap.lcat << ", "
	 << tcatmap.user_ct << ", "
	 << tcatmap.res_ct << ", "
	 << tcatmap.res_gt << "\n";

    /* Send a real echo request now.. */
    dj_autorpc ar(&gs, 1, k, call_ct, djcache);
    dj_gatename arg;
    dj_gatename res;
    arg.gate_ct = 123456;
    arg.gate_id = 654321;
    res.gate_ct = 101;
    res.gate_id = 102;
    c = ar.call(ep, arg, res);
    warn << "autorpc code = " << c << "\n";
    if (c == DELIVERY_DONE)
	warn << "autorpc echo: " << res.gate_ct << "." << res.gate_id << "\n";
}
