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
#include <dj/dis.hh>
#include <dj/djgate.h>
#include <dj/gateutil.hh>

struct incoming_req {
    str nodepk;
    dj_gatename gate;
    djcall_args args;

    dj_reply_status stat;
    djcall_args res;

    uint64_t done;
};

class incoming_impl : public djgate_incoming {
 public:
    incoming_impl(ptr<djprot> p) : p_(p) {
	errno_check(pipe(fds_));
	_make_async(fds_[0]);
	fdcb(fds_[0], selread, wrap(this, &incoming_impl::readcb));
	fd_make_public(fds_[0], 0);

	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.name_ = "djd-incoming";
	gd.func_ = &call_stub;
	gd.arg_ = (void *) this;

	gate_ = gate_create(&gd);
    }

    ~incoming_impl() {
	fdcb(fds_[0], selread, 0);
	close(fds_[0]);
	close(fds_[1]);
	sys_obj_unref(gate_);
    }

    virtual cobj_ref gate() { return gate_; }

 private:
    static void call_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) 
	__attribute__((noreturn))
    {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret);
    }

    void call(gate_call_data *gcd, gatesrv_return *ret) __attribute__((noreturn)) {
	// XXX potentially need to declassify ourselves here..
	uint64_t call_taint = gcd->call_taint;
	uint64_t call_grant = gcd->call_grant;
	uint64_t tct = gcd->taint_container;

	label *cs, *ds, *dr, *nvl, *nvc;

	{ // GC scope
	    incoming_req ir;
	    ir.done = 0;

	    // Get the request and its label
	    label vl, vc;
	    thread_cur_verify(&vl, &vc);

	    str req;
	    dj_gate_call_incoming(gcd->param_obj, vl, vc,
				  call_grant, call_taint,
				  &ir.args.taint,
				  &ir.args.grant,
				  &req);

	    // Unmarshal incoming call request
	    dj_incoming_gate_req igr;
	    if (!str2xdr(igr, req))
		throw basic_exception("cannot unmarshal dj_incoming_gate_req\n");

	    ir.nodepk = str(igr.nodepk.base(), igr.nodepk.size());
	    ir.gate = igr.gate;
	    ir.args.data = str(igr.data.base(), igr.data.size());

	    // XXX what should be the response label if we get an error back?
	    ir.res.taint = label(1);
	    ir.res.grant = label(3);

	    incoming_req *irp = &ir;
	    assert(sizeof(irp) == write(fds_[1], (void *) &irp, sizeof(irp)));
	    while (!ir.done)
		sys_sync_wait(&ir.done, 0, ~0UL);

	    // Construct reply segment
	    dj_incoming_gate_res res;
	    res.set_stat(ir.stat);
	    if (ir.stat == REPLY_DONE)
		*res.data = ir.res.data;

	    str resstr = xdr2str(res);
	    if (!resstr)
		throw basic_exception("cannot encode dj_incoming_gate_res\n");

	    // Labels for return gate call
	    cs = New label(ir.res.taint);
	    ds = New label(ir.res.grant);
	    dr = New label(ir.res.taint);
	    nvl = New label();
	    nvc = New label();

	    dj_gate_call_outgoing(tct, call_grant, call_taint,
				  ir.res.taint, ir.res.grant, resstr,
				  &gcd->param_obj, nvl, nvc);
	}

	ret->ret(cs, ds, dr, nvl, nvc);
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
	p_->call(ir->nodepk, ir->gate, ir->args,
		 wrap(this, &incoming_impl::callcb, ir));
    }

    void callcb(incoming_req *ir, dj_reply_status stat, const djcall_args *res) {
	ir->stat = stat;
	if (res)
	    ir->res = *res;
	ir->done = 1;
	sys_sync_wakeup(&ir->done);
    }

    ptr<djprot> p_;
    cobj_ref gate_;
    int fds_[2];
};

ptr<djgate_incoming>
dj_gate_incoming(ptr<djprot> p)
{
    return New refcounted<incoming_impl>(p);
}
