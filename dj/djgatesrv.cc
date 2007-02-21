extern "C" {
#include <inc/syscall.h>
}

#include <dj/djgatesrv.hh>
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
    obj_get_label(sg, &sl);
    error_check(vl.compare(&sl, label::leq_starlo));
    error_check(sl.compare(&vc, label::leq_starlo));

    /*
     * Read it in and return..
     */
    void *data_map = 0;
    uint64_t len = 0;
    error_check(segment_map(sg, 0, SEGMAP_READ, &data_map, &len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);

    buf2xdr(*m, data_map, len);
    sys_obj_unref(sg);
}
