extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/stack.h>
#include <inc/stdio.h>
}

#include <async.h>
#include <crypt.h>

#include <inc/gatesrv.hh>
#include <inc/errno.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/gateinvoke.hh>
#include <inc/segmentutil.hh>
#include <dj/djgate.h>
#include <dj/gateincoming.hh>
#include <dj/checkpoint.hh>
#include <dj/reqcontext.hh>
#include <dj/djlabel.hh>
#include <dj/mapcreate.hh>
#include <dj/perf.hh>
#include <dj/gatecallstatus.hh>

enum { separate_entry_as = 1 };

struct incoming_req {
    const dj_incoming_gate_req *req;
    dj_delivery_code res;
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

	error_check(sys_self_get_as(&base_as_));

	int64_t entry_asid = sys_as_copy(base_as_, start_env->proc_container,
					 0, "gateincoming-entry");
	error_check(entry_asid);
	cobj_ref entry_as = COBJ(start_env->proc_container, entry_asid);

	checkpoint_update();

	gatesrv_descriptor gd;
	gd.gate_container_ = ct;
	gd.name_ = "djd-incoming";

	if (separate_entry_as) {
	    gd.func_ = &call_entry_stub;
	    gd.as_ = entry_as;
	    gd.flags_ = GATESRV_KEEP_TLS_STACK | GATESRV_NO_THREAD_ADDREF;
	} else {
	    gd.func_ = &call_stub;
	}

	gd.arg_ = (void *) this;
	gd.label_ = &l;
	gd.clearance_ = &c;
	gate_ = gate_create(&gd);
    }

    virtual ~incoming_impl() {
	fdcb(fds_[0], selread, 0);
	close(fds_[0]);
	close(fds_[1]);
	sys_obj_unref(gate_);
    }

    virtual cobj_ref gate() { return gate_; }

 private:
    static void pseudo_gate_call(uint64_t as_ct, uint64_t as_id,
				 uint64_t fn, uint64_t arg)
	__attribute__((noreturn))
    {
	sys_self_set_as(COBJ(as_ct, as_id));
	stack_switch(fn, arg, 0, 0,
		     tls_stack_top, (void*) gatesrv_entry_tls);
    }

    static void call_entry_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	if (start_env->proc_container == i->proc_ct_) {
	    /* Everything is fine, jump into the base AS */
	    pseudo_gate_call(i->base_as_.container, i->base_as_.object,
			     (uint64_t) &call_stub, (uint64_t) i);
	} else {
	    /* Tainted, might as well continue running in this AS.. */
	    call_stub(arg, gcd, ret);
	}
    }

    static void call_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret, false);
    }

    static void call_untaint_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret, true);
    }

    void call(gate_call_data *gcd, gatesrv_return *ret, bool untainted) {
	PERF_COUNTER(gate_incoming::call);

	try {
	    process_call1(gcd, untainted);
	} catch (std::exception &e) {
	    warn << "gateincoming: " << e.what() << "\n";
	}
    }

    void process_call1(gate_call_data *gcd, bool untainted) {
	dj_delivery_code res;
	cobj_ref rseg = gcd->param_obj;

	label vl, vc;
	thread_cur_verify(&vl, &vc);
	if (vl.compare(&vc, label::leq_starlo) < 0)
	    throw basic_exception("bad verify labels %s, %s",
				  vl.to_string(), vc.to_string());

	verify_label_reqctx ctx(vl, vc);

	str reqstr;
	ctx.read_seg(rseg, &reqstr);

	dj_incoming_gate_req req;
	if (!str2xdr(req, reqstr))
	    throw basic_exception("cannot decode dj_incoming_gate_req");

	char digest[20];
	if (!sha1_hashxdr(&digest[0], req))
	    throw basic_exception("cannot hash incoming request");

	if (untainted && memcmp(digest, &gcd->param_buf[16], 20))
	    throw basic_exception("mismatched request after untainting");

	if (!untainted) {
	    try {
		// Acquire the mapping resources provided by caller
		cm_->acquire(req.catmap);

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
		warn << "process_call1: local mapping: " << e.what() << "\n";
		res = DELIVERY_LOCAL_MAPPING;
		return;
	    }

	    if (start_env->proc_container != proc_ct_) {
		sys_self_set_sched_parents(gcd->thread_ref_ct, 0);
		sys_obj_unref(COBJ(start_env->proc_container, thread_id()));

		memcpy(&gcd->param_buf[16], digest, 20);
		stack_switch(base_as_.container, base_as_.object,
			     (uint64_t) &call_untaint_stub, (uint64_t) this,
			     tls_stack_top, (void *) &pseudo_gate_call);
	    }
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

	    int64_t gid = sys_gate_create(start_env->proc_container,
					  0, gl.to_ulabel(), gc.to_ulabel(),
					  0, "gateincoming", 0);
	    if (gid > 0) {
		lms.vl = &vl;
		lms.vc = &vc;
		lms.privgate = COBJ(start_env->proc_container, gid);
		lms_init = true;
	    }
	}

	process_call2(req, &res, lms_init ? (void *) &lms : 0);

	if (lms_init)
	    sys_obj_unref(lms.privgate);

	if (!req.res_ct || !req.res_seg)
	    return;

	cobj_ref ret_obj = COBJ(req.res_ct, req.res_seg);
	if (!ctx.can_rw(ret_obj))
	    return;

	gatecall_status_done(ret_obj, res);
    }

    void process_call2(const dj_incoming_gate_req &req,
		       dj_delivery_code *res,
		       void *local_deliver_arg)
    {
	incoming_req ir;
	ir.req = &req;
	ir.local_deliver_arg = local_deliver_arg;
	ir.done = 0;

	incoming_req *irp = &ir;
	assert(sizeof(irp) == write(fds_[1], (void *) &irp, sizeof(irp)));

	PERF_COUNTER(gate_incoming::wait);
	while (!ir.done)
	    sys_sync_wait(&ir.done, 0, ~0UL);

	*res = ir.res;
    }

    void readcb() {
	PERF_COUNTER(gate_incoming::readcb);

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

    void callcb(incoming_req *ir, dj_delivery_code stat) {
	ir->res = stat;
	ir->done = 1;
	sys_sync_wakeup(&ir->done);
    }

    djprot *p_;
    catmgr *cm_;
    cobj_ref gate_;
    uint64_t proc_ct_;
    int fds_[2];

    cobj_ref base_as_;
};

dj_incoming_gate*
dj_incoming_gate::alloc(djprot *p, catmgr *cm, uint64_t ct)
{
    return New incoming_impl(p, cm, ct);
}
