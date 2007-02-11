extern "C" {
#include <inc/gateparam.h>
}

#include <async.h>
#include <crypt.h>

#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <dj/djgatecall.hh>
#include <dj/djgate.h>
#include <dj/gateutil.hh>

dj_reply_status
djgate_caller::call(str nodepk, dj_gatename gate,
		    const djcall_args &args,
		    djcall_args *resp)
{
    // Construct request
    dj_incoming_gate_req ig_req;
    ig_req.gate = gate;
    ig_req.nodepk.setsize(nodepk.len());
    memcpy(ig_req.nodepk.base(), nodepk.cstr(), nodepk.len());
    ig_req.data.setsize(args.data.len());
    memcpy(ig_req.data.base(), args.data.cstr(), args.data.len());

    str ig_reqstr = xdr2str(ig_req);
    if (!ig_reqstr)
	throw basic_exception("djgate_caller: cannot marshal dj_incoming_gate_req");

    // Marshal request into segment
    gate_call gc(djd_, &args.taint, &args.grant, &args.taint);
    gate_call_data gcd;
    label vl, vc;

    dj_gate_call_outgoing(gc.call_ct(), gc.call_grant(), gc.call_taint(),
			  args.taint, args.grant, ig_reqstr,
			  &gcd.param_obj, &vl, &vc);

    // Off into the djd gate
    gc.call(&gcd, &vl, &vc);
    thread_cur_verify(&vl, &vc);

    // Parse reply
    str ig_resstr;
    dj_gate_call_incoming(gcd.param_obj, vl, vc,
			  gc.call_grant(), gc.call_taint(),
			  &resp->taint, &resp->grant, &ig_resstr);

    dj_incoming_gate_res ig_res;
    if (!str2xdr(ig_res, ig_resstr))
	throw basic_exception("djgate_caller: cannot unmarshal dj_incoming_gate_res");

    if (ig_res.stat == REPLY_DONE)
	resp->data = str(ig_res.data->base(), ig_res.data->size());
    else
	resp->data = str();

    return ig_res.stat;
}
