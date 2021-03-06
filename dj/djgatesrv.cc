extern "C" {
#include <inc/syscall.h>
}

#include <dj/djgatesrv.hh>
#include <dj/djlabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

void
djgate_incoming(gate_call_data *gcd,
		const label &vl, const label &vc,
		dj_outgoing_gate_msg *m, gatesrv_return *r)
{
    cobj_ref sg = gcd->param_obj;

    /*
     * Zap the return gate to avoid leaking anything back to a
     * malicious caller; the alternative is to ensure that every
     * return call taints our caller (return target) with vl, but
     * a real exporter doesn't create a return gate anyway.
     */
    r->change_gate(COBJ(0, 0));

    /*
     * Verify that we aren't being tricked into reading something..
     */
    label sl;
    try {
	obj_get_label(COBJ(sg.container, sg.container), &sl);
	error_check(vl.compare(&sl, label::leq_starlo));
	error_check(sl.compare(&vc, label::leq_starlo));
 
	obj_get_label(sg, &sl);
	error_check(vl.compare(&sl, label::leq_starlo));
	error_check(sl.compare(&vc, label::leq_starlo));
    } catch (std::exception &e) {
	warn << "djgate_incoming: " << e.what() << "\n";
	warn << "vl = " << vl.to_string() << "\n";
	warn << "sl = " << sl.to_string() << "\n";
	warn << "vc = " << vc.to_string() << "\n";
	throw;
    }

    /*
     * Read it in
     */
    void *data_map = 0;
    uint64_t len = 0;
    error_check(segment_map(sg, 0, SEGMAP_READ, &data_map, &len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);

    buf2xdr(*m, data_map, len);
    sys_obj_unref(sg);

    /*
     * Verify that the labels make sense
     */
    dj_catmap_indexed mi(m->m.catmap);
    label mt, mg, mc;
    djlabel_to_label(mi, m->m.taint, &mt, label_taint);
    djlabel_to_label(mi, m->m.glabel, &mg, label_owner);
    djlabel_to_label(mi, m->m.gclear, &mc, label_clear);

    error_check(vl.compare(&mg, label::leq_starlo));
    error_check(vl.compare(&mt, label::leq_starlo));
    error_check(mt.compare(&vc, label::leq_starhi));
    error_check(mc.compare(&vc, label::leq_starhi));
}
