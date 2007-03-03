extern "C" {
#include <inc/syscall.h>
#include <inc/stack.h>
}

#include <inc/gateinvoke.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <dj/djsrpc.hh>
#include <dj/djlabel.hh>
#include <dj/djrpcx.h>

static void __attribute__((noreturn))
dj_rpc_srv_return(cobj_ref *gp, label *tgtl, label *tgtc, gatesrv_return *r)
{
    gate_invoke(*gp, tgtl, tgtc, &gatesrv_return::cleanup_stub, r);
}

void
dj_rpc_srv(dj_rpc_service_fn *fn,
	   gate_call_data *gcd, gatesrv_return *ret)
{
    cobj_ref djd_gate;
    uint64_t djcall_ct = gcd->param_obj.container;

    label *tgtl = New label();
    scope_guard<void, label*> delete_l(delete_obj, tgtl);

    label *tgtc = New label();
    scope_guard<void, label*> delete_c(delete_obj, tgtc);

    { // GC scope
	label vl, vc;
	thread_cur_verify(&vl, &vc);

	dj_outgoing_gate_msg reqmsg;
	djgate_incoming(gcd, vl, vc, &reqmsg, ret);
	sys_obj_unref(gcd->param_obj);

	djd_gate.container = reqmsg.djd_gate.gate_ct;
	djd_gate.object = reqmsg.djd_gate.gate_id;

	dj_call_msg cm;
	if (!bytes2xdr(cm, reqmsg.m.msg)) {
	    printf("dj_rpc_srv: cannot unmarshal dj_call_msg\n");
	    return;
	}

	dj_rpc_reply reply;
	reply.sender = reqmsg.sender;
	reply.tmo = 0;
	reply.dset = reqmsg.m.dset;
	reply.catmap = reqmsg.m.catmap;

	reply.msg.target = cm.return_ep;
	reply.msg.token = thread_id();
	reply.msg.taint = reqmsg.m.taint;
	reply.msg.glabel = reqmsg.m.glabel;
	reply.msg.gclear = reqmsg.m.gclear;
	reply.msg.catmap = cm.return_cm;
	reply.msg.dset = cm.return_ds;

	if (!fn(reqmsg.m, str(cm.buf.base(), cm.buf.size()), &reply)) {
	    printf("dj_rpc_srv: service function is bummed out\n");
	    return;
	}

	dj_incoming_gate_req replymsg;
	replymsg.node = reply.sender;
	replymsg.timeout = reply.tmo;
	replymsg.dset = reply.dset;
	replymsg.catmap = reply.catmap;
	replymsg.m = reply.msg;

	/*
	 * Compute return real labels
	 */
	dj_catmap_indexed cmi(replymsg.catmap);
	label tlabel, glabel, gclear;
	djlabel_to_label(cmi, replymsg.m.taint, &tlabel, label_taint);
	djlabel_to_label(cmi, replymsg.m.glabel, &glabel, label_owner);
	djlabel_to_label(cmi, replymsg.m.gclear, &gclear, label_clear);

	gate_compute_labels(djd_gate, &tlabel, &glabel, &gclear, tgtl, tgtc);

	str replystr = xdr2str(replymsg);
	void *data_map = 0;
	error_check(segment_alloc(djcall_ct, replystr.len(), &gcd->param_obj,
				  &data_map, tlabel.to_ulabel(),
				  "djsrpc reply"));
	scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
	memcpy(data_map, replystr.cstr(), replystr.len());

	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);
	sys_self_set_verify(tl.to_ulabel(), tc.to_ulabel());
    }

    delete_l.dismiss();
    delete_c.dismiss();

    /*
     * Make sure that djd knows where the thread is anchored, when
     * its gatesrv code wants to call sys_self_sched_parents().
     */
    sys_self_set_sched_parents(start_env->proc_container, djcall_ct);
    gcd->taint_container = djcall_ct;

    stack_switch((uint64_t) &djd_gate,
		 (uint64_t) tgtl,
		 (uint64_t) tgtc,
		 (uint64_t) ret,
		 tls_stack_top, (void *) &dj_rpc_srv_return);
}
