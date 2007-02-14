extern "C" {
#include <inc/container.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <async.h>

#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <dj/gateutil.hh>

void
dj_gate_call_incoming(const cobj_ref &seg, const label &vl, const label &vc,
		      uint64_t call_grant, uint64_t call_taint,
		      label *argtaint, label *arggrant, str *data)
{
    if (dj_label_debug)
	warn << "dj_gate_call_incoming: vl " << vl.to_string()
	     << ", vc " << vc.to_string() << "\n";

    label l;
    obj_get_label(COBJ(seg.container, seg.container), &l);
    if (dj_label_debug)
	warn << "dj_gate_call_incoming: ct label " << l.to_string() << "\n";
    error_check(vl.compare(&l, label::leq_starlo));
    error_check(l.compare(&vc, label::leq_starhi));

    obj_get_label(seg, &l);
    if (dj_label_debug)
	warn << "dj_gate_call_incoming: seg label " << l.to_string() << "\n";
    error_check(vl.compare(&l, label::leq_starlo));
    error_check(l.compare(&vc, label::leq_starhi));

    *argtaint = l;
    argtaint->set(call_taint, argtaint->get_default());
    argtaint->set(call_grant, argtaint->get_default());
    *arggrant = vl;
    arggrant->set(call_taint, 3);
    arggrant->set(call_grant, 3);
    arggrant->transform(label::nonstar_to, 3);

    void *data_map = 0;
    uint64_t data_len = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ,
			    &data_map, &data_len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    data->setbuf((const char *) data_map, data_len);
}

void
dj_gate_call_outgoing(uint64_t ct, uint64_t call_grant, uint64_t call_taint,
		      const label &taint, const label &grant, const str &data,
		      cobj_ref *segp, label *vl, label *vc)
{
    label seglabel(taint);
    seglabel.set(call_grant, 0);
    seglabel.set(call_taint, 3);

    int64_t nct = sys_container_alloc(ct, seglabel.to_ulabel_const(),
				      "dj_gate_call_outgoing ct", 0, CT_QUOTA_INF);
    if (nct < 0) {
	if (dj_label_debug) {
	    label tl, ctl;
	    thread_cur_label(&tl);
	    obj_get_label(COBJ(ct, ct), &ctl);

	    warn << "dj_gate_call_outgoing: ct alloc: " << e2s(nct) << "\n";
	    warn << "parent ct label: " << ctl.to_string() << "\n";
	    warn << "thread label: " << tl.to_string() << "\n";
	    warn << "new ct label: " << seglabel.to_string() << "\n";
	}

	error_check(nct);
    }

    void *data_map = 0;
    error_check(segment_alloc(nct, data.len(), segp, &data_map, 0,
			      "dj_gate_call_outgoing args"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    memcpy(data_map, data.cstr(), data.len());

    grant.merge(&taint, vl, label::min, label::leq_starlo);
    *vc = taint;

    vl->set(call_grant, LB_LEVEL_STAR);
    vl->set(call_taint, LB_LEVEL_STAR);
    vc->set(call_grant, 3);
    vc->set(call_taint, 3);
}

void
dj_gate_call_save_taint(uint64_t ct, gate_call_data *gcd, cobj_ref *segp)
{
    dj_gate_call_tainted t;
    t.seg = gcd->param_obj;
    t.rgate = gcd->return_gate;
    t.taint_ct = gcd->taint_container;
    t.call_grant = gcd->call_grant;
    t.call_taint = gcd->call_taint;

    label vl, vc;
    thread_cur_verify(&vl, &vc);

    const ulabel *uvl = vl.to_ulabel_const();
    const ulabel *uvc = vc.to_ulabel_const();
    t.vl = *uvl;
    t.vc = *uvc;

    label seglabel;
    thread_cur_label(&seglabel);
    seglabel.transform(label::star_to, seglabel.get_default());
    seglabel.set(start_env->process_grant, 0);
    seglabel.set(start_env->process_taint, 3);

    dj_gate_call_tainted *tp = 0;
    uint64_t len = sizeof(t) + (uvl->ul_nent + uvc->ul_nent) * sizeof(uint64_t);
    error_check(segment_alloc(ct, len, segp, (void **) &tp,
			      seglabel.to_ulabel_const(),
			      "dj_gate_call_save_taint"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, tp, 1);
    memcpy(tp, &t, sizeof(t));
    memcpy(&tp->labelent[0], &uvl->ul_ent[0], uvl->ul_nent * sizeof(uint64_t));
    memcpy(&tp->labelent[uvl->ul_nent], &uvc->ul_ent[0], uvc->ul_nent * sizeof(uint64_t));
}

void
dj_gate_call_load_taint(cobj_ref taintseg, gate_call_data *gcd,
			label *vl, label *vc)
{
    dj_gate_call_tainted *tp = 0;
    error_check(segment_map(taintseg, 0, SEGMAP_READ, (void **) &tp, 0, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, tp, 1);

    gcd->param_obj = tp->seg;
    gcd->return_gate = tp->rgate;
    gcd->taint_container = tp->taint_ct;
    gcd->call_grant = tp->call_grant;
    gcd->call_taint = tp->call_taint;

    ulabel uvl, uvc;
    uvl = tp->vl;
    uvc = tp->vc;

    uvl.ul_ent = &tp->labelent[0];
    uvc.ul_ent = &tp->labelent[uvl.ul_nent];

    vl->from_ulabel(&uvl);
    vc->from_ulabel(&uvc);
}
