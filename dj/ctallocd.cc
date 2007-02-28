extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <async.h>
#include <inc/labelutil.hh>
#include <dj/djgatesrv.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>
#include <dj/reqcontext.hh>
#include <dj/miscx.h>

static cobj_ref base_as;

static void
killer_thread(uint64_t kill_at, uint64_t ctparent, uint64_t ct,
				uint64_t tparent, uint64_t tid)
{
    for (;;) {
	uint64_t now = sys_clock_msec();
	if (now > kill_at)
	    break;

	sys_sync_wait(&now, now, kill_at);
    }

    sys_obj_unref(COBJ(ctparent, ct));
    sys_obj_unref(COBJ(tparent, tid));
    sys_self_halt();
}

static bool
ct_alloc_service(const dj_message &m, const str &s, dj_rpc_reply *r)
try {
    container_alloc_req req;
    container_alloc_res res;
    if (!str2xdr(req, s)) {
	warn << "ct_alloc: str2xdr\n";
	return false;
    }

    dj_catmap_indexed cmi(m.catmap);
    label lt, lg, lc;
    djlabel_to_label(cmi, m.taint, &lt, label_taint);
    djlabel_to_label(cmi, m.glabel, &lg, label_owner);
    djlabel_to_label(cmi, m.gclear, &lc, label_clear);

    label lng, lnc;
    lt.merge(&lg, &lng, label::min, label::leq_starlo);
    lt.merge(&lc, &lnc, label::max, label::leq_starlo);

    verify_label_reqctx ctx(lng, lnc);
    if (!ctx.can_rw(COBJ(req.parent, req.parent))) {
	warn << "ct_alloc: no permission\n";
	return false;
    }

    label ctl;
    djlabel_to_label(cmi, req.label, &ctl, label_taint);
    error_check(lng.compare(&ctl, label::leq_starlo));
    error_check(ctl.compare(&lnc, label::leq_starhi));

    res.ct_id = sys_container_alloc(req.parent, ctl.to_ulabel(),
				    "ctallocd", 0, req.quota);
    if (res.ct_id < 0) {
	warn << "ct_alloc: sys_container_alloc: " << e2s(res.ct_id) << "\n";
	goto out;
    }

    if (req.timeout_msec) {
	int64_t tparent = res.ct_id;
	int64_t tid = sys_thread_create(tparent, "killer-thread");

	if (tid < 0) {
	    tparent = req.parent;
	    tid = sys_thread_create(tparent, "killer-thread");
	}

	if (tid < 0) {
	    warn << "ct_alloc: sys_thread_create: " << e2s(tid) << "\n";
	    sys_obj_unref(COBJ(req.parent, res.ct_id));
	    res.ct_id = tid;
	    goto out;
	}

	thread_entry te;
	te.te_entry = (void *) &killer_thread;
	te.te_stack = tls_stack_top;
	te.te_as = base_as;
	te.te_arg[0] = sys_clock_msec() + req.timeout_msec;
	te.te_arg[1] = req.parent;
	te.te_arg[2] = res.ct_id;
	te.te_arg[3] = tparent;
	te.te_arg[4] = tid;

	int r = sys_thread_start(COBJ(tparent, tid), &te, 0, 0);
	if (r < 0) {
	    warn << "ct_alloc: sys_thread_start: " << e2s(r) << "\n";
	    sys_obj_unref(COBJ(tparent, tid));
	    sys_obj_unref(COBJ(req.parent, res.ct_id));
	    res.ct_id = r;
	    goto out;
	}
    }

 out:
    r->msg.msg = xdr2str(res);
    return true;
} catch (std::exception &e) {
    warn << "ct_alloc: " << e.what() << "\n";
    return false;
}

static void
gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(ct_alloc_service, gcd, r);
}

int
main(int ac, char **av)
{
    error_check(sys_self_get_as(&base_as));

    label lpub(1);

    int64_t call_ct;
    error_check(call_ct = sys_container_alloc(start_env->shared_container,
					      lpub.to_ulabel(), "public call",
					      0, 10 * 1024 * 1024));

    warn << "ctallocd public container: " << call_ct << "\n";

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "ctallocd";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    warn << "ctallocd gate: " << g << "\n";

    thread_halt();
}
