#include <dj/djrpcx.h>
#include <dj/djprotx.h>
#include <dj/internalx.h>
#include <dj/miscx.h>
#include <dj/djarpc.hh>
#include <dj/djflume.hh>

extern "C" {
namespace flume {
#include "flume_cpp.h"
#include "flume_prot.h"
#include "flume_api.h"
#include "flume_clnt.h"
}
}

/*
 * XXX
 *
 * Right now nothing enforces the integrity of mappings.
 * We also don't have any mechanism for charging the user
 * for the mapping's resources, and the exporter's label
 * keeps growing.
 *
 * Should make mappings opaque in the protocol, so we can
 * add whatever we want to them (e.g. signature).
 */

void
flume_mapcreate::exec(const dj_message &m, const delivery_args &da)
{
    if (m.target.type != EP_MAPCREATE) {
	warn << "flume_mapcreate: not a mapcreate target\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_call_msg callmsg;
    dj_mapcreate_arg maparg;
    if (!bytes2xdr(callmsg, m.msg) || !bytes2xdr(maparg, callmsg.buf)) {
	warn << "flume_mapcreate: cannot unmarshal\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    if (callmsg.return_ep.type != EP_GATE &&
	callmsg.return_ep.type != EP_SEGMENT) {
	warn << "flume_mapcreate: must return to a gate or segment\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_mapcreate_res mapres;
    for (uint32_t i = 0; i < maparg.reqs.size(); i++) {
	const dj_mapreq &mapreq = maparg.reqs[i];

	dj_gcat gcat = mapreq.gcat;
	uint64_t lcat = mapreq.lcat;

	if (mapreq.lcat) {
	    warn << "flume_mapcreate: cannot create global categories\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	flume::x_handle_t h;
	int rc;
	if ((rc = flume::flume_new_handle(&h, 0, "mapped handle")) < 0) {
	    warn << "flume_mapcreate: cannot allocate a handle\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	dj_cat_mapping mapent;
	mapent.gcat = mapreq.gcat;
	mapent.lcat = h;
	mapent.res_ct = 0;
	mapent.res_gt = 0;
	mapres.mappings.push_back(mapent);
    }

    dj_message replym;
    replym.to = m.from;
    replym.target = callmsg.return_ep;
    replym.taint = m.taint;
    replym.catmap = callmsg.return_cm;
    replym.dset = callmsg.return_ds;
    replym.msg = xdr2str(mapres);

    p_->send(replym, m.dset, 0, 0);
    da.cb(DELIVERY_DONE);
}

void
dj_flume_perl_svc(flume_mapcreate *fmc,
		  const dj_message &m, const str &s,
		  const dj_arpc_reply &ar)
{
    /*
     * Use fmc to verify the mappings..
     */
    dj_rpc_reply rr = ar.r;

    perl_run_arg parg;
    perl_run_res pres;

    if (!str2xdr(parg, s)) {
	warn << "dj_flume_perl_svc: cannot decode\n";
	ar.cb(false, rr);
	return;
    }

    str input = parg.input;
    str script = parg.script;
    str output;
    int errcode;

    /* magic happens here! */

    output = "This is only a dummy reply.\n";
    errcode = 0;

    pres.retval = errcode;
    pres.output = output;

    rr.msg.msg = xdr2str(pres);
    ar.cb(true, rr);
}
