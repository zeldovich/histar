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
#include <dj/djutil.hh>
#include <dj/miscx.h>

int
main(int ac, char **av)
{
    dj_global_cache djcache;

    if (ac != 4) {
	printf("Usage: %s host-pk call-ct perl-gate\n", av[0]);

	try {
	    gate_sender gs;
	    warn << "Local key: " << gs.hostkey() << "\n";
	} catch (...) {}

	exit(-1);
    }

    gate_sender gs;

    str pkstr(av[1]);
    dj_pubkey k;
    if (pkstr == "0") {
	k = gs.hostkey();
    } else {
	ptr<sfspub> sfspub = sfscrypt.alloc(pkstr, SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	k = sfspub2dj(sfspub);
    }

    uint64_t call_ct = atoi(av[2]);

    dj_gatename service_gate;
    service_gate <<= av[3];

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

    /* Allocate mappings & delegations */
    dj_cat_mapping local_cme;
    dj_cat_mapping remote_cme;

    try {
	label ct_local(1), ct_remote(1);

	dj_stmt_signed dres;
	dj_map_and_delegate(tcat, false,
			    ct_local, ct_remote,
			    local_ct, call_ct, k,
			    &gs, djcache,
			    &local_cme, &remote_cme, &dres);
    } catch (std::exception &e) {
	warn << "dj_map_and_delegate: " << e.what() << "\n";
    }

    /* Create a remote tainted container */
    dj_message_endpoint ctalloc_ep;
    ctalloc_ep.set_type(EP_GATE);
    ctalloc_ep.ep_gate->msg_ct = call_ct;
    ctalloc_ep.ep_gate->gate.gate_ct = 0;
    ctalloc_ep.ep_gate->gate.gate_id = GSPEC_CTALLOC;

    container_alloc_req ctreq;
    container_alloc_res ctres;

    ctreq.parent = call_ct;
    ctreq.quota = CT_QUOTA_INF;
    ctreq.timeout_msec = 5000;
    ctreq.label.ents.push_back(local_cme.gcat);

    label xgrant(3);
    xgrant.set(tcat, LB_LEVEL_STAR);

    label xclear(0);
    xclear.set(tcat, 3);

    dj_delivery_code c;
    dj_autorpc remote_ar(&gs, 5, k, djcache);
    c = remote_ar.call(ctalloc_ep, ctreq, ctres, 0, &xgrant, &xclear);
    if (c != DELIVERY_DONE)
	warn << "error from ctalloc: code " << c << "\n";
    warn << "New remote container: " << ctres.ct_id << "\n";

    /* Send a real request now.. */
    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = ctres.ct_id;
    ep.ep_gate->gate = service_gate;

    perl_run_arg parg;
    perl_run_res pres;
    parg.script = str("print 'A'x5; print <>;");
    parg.input = str("Hello world.");

    label taint(1);
    taint.set(tcat, 3);

    c = remote_ar.call(ep, parg, pres, &taint);
    warn << "Server response code = " << c << "\n";
    if (c == DELIVERY_DONE) {
	warn << "Perl exit code: " << pres.retval << "\n";
	warn << "Perl output: " << pres.output << "\n";
    }
}
