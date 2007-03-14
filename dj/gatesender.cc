extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <crypt.h>
#include <inc/scopeguard.hh>
#include <inc/goblegateclnt.hh>
#include <inc/labelutil.hh>
#include <dj/gatesender.hh>
#include <dj/djlabel.hh>
#include <dj/djgate.h>
#include <dj/gatecallstatus.hh>

dj_delivery_code
gate_sender::send(const dj_pubkey &node, time_t timeout,
		  const dj_delegation_set &dset, const dj_catmap &cm,
		  const dj_message &msg, label *grantlabel)
{
    dj_incoming_gate_req req;
    req.node = node;
    req.timeout = timeout;
    req.dset = dset;
    req.catmap = cm;
    req.m = msg;

    dj_catmap_indexed cmi(cm);
    label tlabel, glabel, gclear;
    djlabel_to_label(cmi, msg.taint, &tlabel, label_taint);
    djlabel_to_label(cmi, msg.glabel, &glabel, label_owner);
    djlabel_to_label(cmi, msg.gclear, &gclear, label_clear);

    if (grantlabel) {
	label tmp;
	glabel.merge(grantlabel, &tmp, label::min, label::leq_starlo);
	glabel = tmp;
    }

    goblegate_call gc(g_, &tlabel, &glabel, &gclear, true);

    label reqlabel(tlabel);
    reqlabel.set(gc.call_grant(), 0);
    reqlabel.set(gc.call_taint(), 3);
    
    cobj_ref gcs_obj = gatecall_status_alloc(gc.call_ct(), &reqlabel);
    req.res_ct = gcs_obj.container;
    req.res_seg = gcs_obj.object;
     
    str reqstr = xdr2str(req);
    gate_call_data gcd;

    void *data_map = 0;
    error_check(segment_alloc(gc.call_ct(), reqstr.len(), &gcd.param_obj,
			      &data_map, reqlabel.to_ulabel(), "gate_sender req"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    memcpy(data_map, reqstr.cstr(), reqstr.len());

    label vl, vc;
    thread_cur_label(&vl);
    thread_cur_clearance(&vc);
    gc.call(&gcd, &vl, &vc);

    return gatecall_status_wait(gcs_obj);
}
