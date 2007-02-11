extern "C" {
#include <inc/container.h>
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
    label l;
    obj_get_label(COBJ(seg.container, seg.container), &l);
    error_check(vl.compare(&l, label::leq_starlo));
    error_check(l.compare(&vc, label::leq_starhi));

    obj_get_label(seg, &l);
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

    void *data_map = 0;
    error_check(segment_alloc(ct, data.len(), segp, &data_map,
			      seglabel.to_ulabel_const(),
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
