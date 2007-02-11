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

    label seglabel(args.taint);
    seglabel.set(gc.call_grant(), 0);
    seglabel.set(gc.call_taint(), 3);

    cobj_ref data_seg;
    void *data_map = 0;
    error_check(segment_alloc(gc.call_ct(), ig_reqstr.len(),
			      &data_seg, &data_map,
			      seglabel.to_ulabel_const(),
			      "djgate_caller args"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    memcpy(data_map, ig_reqstr.cstr(), ig_reqstr.len());

    // Off into the djd gate
    gate_call_data gcd;
    gcd.param_obj = data_seg;

    label vl, vc;
    args.grant.merge(&args.taint, &vl, label::min, label::leq_starlo);
    vc = args.taint;

    gc.call(&gcd, &vl, &vc);
    thread_cur_verify(&vl, &vc);

    // Parse reply
    dj_incoming_gate_res ig_res;
    data_seg = gcd.param_obj;

    label l;
    obj_get_label(COBJ(data_seg.container, data_seg.container), &l);
    error_check(vl.compare(&l, label::leq_starlo));
    error_check(l.compare(&vc, label::leq_starhi));
    obj_get_label(data_seg, &l);
    error_check(vl.compare(&l, label::leq_starlo));
    error_check(l.compare(&vc, label::leq_starhi));

    resp->taint = l;
    resp->taint.set(gc.call_taint(), resp->taint.get_default());
    resp->taint.set(gc.call_grant(), resp->taint.get_default());
    resp->grant = vl;
    resp->grant.set(gc.call_taint(), 3);
    resp->grant.set(gc.call_grant(), 3);
    resp->grant.transform(label::nonstar_to, 3);

    data_map = 0;
    uint64_t data_len = 0;
    error_check(segment_map(data_seg, 0, SEGMAP_READ,
			    &data_map, &data_len, 0));
    scope_guard2<int, void*, int> unmap2(segment_unmap_delayed, data_map, 1);
    str ig_resstr((const char *) data_map, data_len);
    unmap2.force();

    if (!str2xdr(ig_res, ig_resstr))
	throw basic_exception("djgate_caller: cannot unmarshal dj_incoming_gate_res");

    if (ig_res.stat == REPLY_DONE)
	resp->data = str(ig_res.data->base(), ig_res.data->size());
    else
	resp->data = str();

    return ig_res.stat;
}
