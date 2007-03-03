extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/fd.h>
}

#include <async.h>
#include <crypt.h>

#include <inc/gatesrv.hh>
#include <inc/errno.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/gateinvoke.hh>
#include <dj/djgate.h>
#include <dj/gateincoming.hh>
#include <dj/checkpoint.hh>
#include <dj/reqcontext.hh>
#include <dj/djlabel.hh>
#include <dj/mapcreate.hh>

struct incoming_req {
    const dj_incoming_gate_req *req;
    dj_incoming_gate_res res;
    void *local_deliver_arg;
    uint64_t done;
};

class incoming_impl : public dj_incoming_gate {
 public:
    incoming_impl(djprot *p, catmgr *cm, uint64_t ct)
	: p_(p), cm_(cm), gate_(COBJ(0, 0)),
	  proc_ct_(start_env->proc_container)
    {
	errno_check(pipe(fds_));
	_make_async(fds_[0]);
	fdcb(fds_[0], selread, wrap(this, &incoming_impl::readcb));
	fd_make_public(fds_[0], 0);
	fd_make_public(fds_[1], 0);

	label tl;
	thread_cur_label(&tl);

	label l(1);
	l.set(start_env->process_grant, LB_LEVEL_STAR);
	l.set(start_env->process_taint, LB_LEVEL_STAR);

	struct Fd *fd;
	error_check(fd_lookup(fds_[1], &fd, 0, 0));
	for (int j = 0; j < fd_handle_max; j++)
	    if (fd->fd_handle[j] && tl.get(fd->fd_handle[j]) == LB_LEVEL_STAR)
		l.set(fd->fd_handle[j], LB_LEVEL_STAR);

	for (int i = 0; i <= 2; i++) {
	    struct Fd *fd;
	    fd_make_public(i, 0);
	    if (fd_lookup(i, &fd, 0, 0) == 0)
		for (int j = 0; j < fd_handle_max; j++)
		    if (fd->fd_handle[j] && tl.get(fd->fd_handle[j]) == LB_LEVEL_STAR)
			l.set(fd->fd_handle[j], LB_LEVEL_STAR);
	}

	label c(l);
	c.transform(label::nonstar_to, 2);
	c.transform(label::star_to, 3);

	label v(3);
	v.set(start_env->process_grant, 0);

	gatesrv_descriptor gdi;
	gdi.gate_container_ = ct;
	gdi.name_ = "djd-untainted";
	gdi.func_ = &call_untaint_stub;
	gdi.arg_ = (void *) this;
	gdi.label_ = &l;
	gdi.clearance_ = &c;
	gdi.verify_ = &v;
	untaint_gate_ = gate_create(&gdi);

	checkpoint_update();

	gatesrv_descriptor gde;
	gde.gate_container_ = ct;
	gde.name_ = "djd-incoming";
	gde.func_ = &call_stub;
	gde.arg_ = (void *) this;
	gde.label_ = &l;
	gde.clearance_ = &c;
	gate_ = gate_create(&gde);
    }

    virtual ~incoming_impl() {
	fdcb(fds_[0], selread, 0);
	close(fds_[0]);
	close(fds_[1]);
	sys_obj_unref(untaint_gate_);
	sys_obj_unref(gate_);
    }

    virtual cobj_ref gate() { return gate_; }

 private:
    static void call_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret, false);
    }

    static void call_untaint_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret, true);
    }

    void call(gate_call_data *gcd, gatesrv_return *ret, bool untainted) {
	bool halt = false;
	label *cs = 0;
	process_call1(gcd, &cs, &halt, untainted);
	if (!halt)
	    ret->ret(cs, 0, 0);

	sys_obj_unref(COBJ(gcd->taint_container, sys_self_id()));
    }

    void process_call1(gate_call_data *gcd, label **csp, bool *haltp, bool untainted) {
	dj_incoming_gate_res res;

	try {
	    process_call2(gcd, csp, haltp, untainted, &res);
	} catch (std::exception &e) {
	    warn << "incoming_impl::process_call1: " << e.what() << "\n";
	    res.set_stat(DELIVERY_LOCAL_ERR);
	}

	str s = xdr2str(res);
	if (s.len() > sizeof(gcd->param_buf)) {
	    warn << "incoming_impl::process_call1: encoded response size too large!\n";
	} else {
	    memcpy(&gcd->param_buf[0], s.cstr(), s.len());
	}
    }

    void process_call2(gate_call_data *gcd, label **csp, bool *haltp,
		       bool untainted, dj_incoming_gate_res *res)
    {
	cobj_ref rseg = gcd->param_obj;
	if (!untainted)
	    error_check(sys_obj_set_readonly(rseg));

	label vl, vc;
	thread_cur_verify(&vl, &vc);
	if (vl.compare(&vc, label::leq_starlo) < 0)
	    throw basic_exception("bad verify labels %s, %s",
				  vl.to_string(), vc.to_string());

	*csp = New label(vl);
	verify_label_reqctx ctx(vl, vc);

	str reqstr;
	ctx.read_seg(rseg, &reqstr);

	dj_incoming_gate_req req;
	if (!str2xdr(req, reqstr))
	    throw basic_exception("cannot decode dj_incoming_gate_req");

	if (!untainted) {
	    try {
		label temp_vl(3);
		label temp_vc(0);
		sys_self_set_verify(temp_vl.to_ulabel(), temp_vc.to_ulabel());

		// Verify & acquire the mapping resources provided by caller
		cm_->acquire(req.catmap);
		cm_->resource_check(&ctx, req.catmap);

		// Make sure global labels are within the caller's authority
		dj_catmap_indexed mi(req.catmap);
		label mt, mg, mc;
		djlabel_to_label(mi, req.m.taint, &mt, label_taint);
		djlabel_to_label(mi, req.m.glabel, &mg, label_owner);
		djlabel_to_label(mi, req.m.gclear, &mc, label_clear);

		error_check(vl.compare(&mg, label::leq_starlo));
		error_check(vl.compare(&mt, label::leq_starlo));
		error_check(mt.compare(&vc, label::leq_starhi));
		error_check(mc.compare(&vc, label::leq_starhi));
	    } catch (std::exception &e) {
		warn << "process_call2: local mapping: " << e.what() << "\n";
		res->set_stat(DELIVERY_LOCAL_MAPPING);
		return;
	    }

	    if (start_env->proc_container != proc_ct_) {
		label tl, tc;
		thread_cur_label(&tl);
		thread_cur_clearance(&tc);

		label nvl, nvc;
		vl.merge(&tl, &nvl, label::max, label::leq_starlo);
		vc.merge(&tc, &nvc, label::min, label::leq_starhi);
		sys_self_set_verify(nvl.to_ulabel(), nvc.to_ulabel());

		label tgtl, tgtc;
		gate_compute_labels(untaint_gate_, 0, &tl, &tc, &tgtl, &tgtc);
		sys_gate_enter(untaint_gate_, tgtl.to_ulabel(), tgtc.to_ulabel(), 0);
		fatal << "incoming_impl::call: untainting gate call returned\n";
	    }
	}

	if (req.m.token) {
	    int64_t tid = sys_self_id();
	    if (tid >= 0 && req.m.token == (uint64_t) tid)
		*haltp = true;
	    else
		throw basic_exception("refusing bad token");
	}

	bool lms_init = false;
	local_mapcreate_state lms;
	if (req.m.target.type == EP_MAPCREATE && req.node == p_->pubkey()) {
	    label tl, tc;
	    thread_cur_label(&tl);
	    thread_cur_clearance(&tc);

	    label gl, gc;
	    tl.merge(&vl, &gl, label::max, label::leq_starlo);
	    tc.merge(&vc, &gc, label::min, label::leq_starlo);

	    label gv(3);
	    gv.set(start_env->process_grant, 0);

	    int64_t gid = sys_gate_create(start_env->proc_container,
					  0, gl.to_ulabel(), gc.to_ulabel(),
					  gv.to_ulabel(), "gateincoming", 0);
	    if (gid > 0) {
		lms.vl = &vl;
		lms.vc = &vc;
		lms.privgate = COBJ(start_env->proc_container, gid);
		lms_init = true;
	    }
	}

	process_call3(req, res, lms_init ? (void *) &lms : 0);

	if (lms_init)
	    sys_obj_unref(lms.privgate);
    }

    void process_call3(const dj_incoming_gate_req &req, dj_incoming_gate_res *res,
		       void *local_deliver_arg)
    {
	incoming_req ir;
	ir.req = &req;
	ir.local_deliver_arg = local_deliver_arg;
	ir.done = 0;

	incoming_req *irp = &ir;
	assert(sizeof(irp) == write(fds_[1], (void *) &irp, sizeof(irp)));
	while (!ir.done)
	    sys_sync_wait(&ir.done, 0, ~0UL);

	*res = ir.res;
    }

    void readcb() {
	incoming_req *ir;
	ssize_t cc = read(fds_[0], (void *) &ir, sizeof(ir));
	if (cc < 0) {
	    warn << "incoming_impl::readcb: " << strerror(errno) << "\n";
	    return;
	}

	if (cc == 0) {
	    warn << "incoming_impl::readcb: EOF\n";
	    fdcb(fds_[0], selread, 0);
	    return;
	}

	assert(cc == sizeof(ir));
	p_->send(ir->req->node, ir->req->timeout, ir->req->dset, ir->req->m,
		 wrap(this, &incoming_impl::callcb, ir), ir->local_deliver_arg);
    }

    void callcb(incoming_req *ir, dj_delivery_code stat, uint64_t token) {
	ir->res.set_stat(stat);
	if (stat == DELIVERY_DONE)
	    *ir->res.token = token;

	ir->done = 1;
	sys_sync_wakeup(&ir->done);
    }

    djprot *p_;
    catmgr *cm_;
    cobj_ref untaint_gate_;
    cobj_ref gate_;
    uint64_t proc_ct_;
    int fds_[2];
};

dj_incoming_gate*
dj_incoming_gate::alloc(djprot *p, catmgr *cm, uint64_t ct)
{
    return New incoming_impl(p, cm, ct);
}
