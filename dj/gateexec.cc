extern "C" {
#include <inc/atomic.h>
#include <inc/utrap.h>
#include <inc/gateparam.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/memlayout.h>
}

#include <dj/dis.hh>
#include <dj/gateutil.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/gateinvoke.hh>

#define GATE_EXEC_CALL		0
#define GATE_EXEC_RETURN	1
#define GATE_EXEC_ABORT		2
#define GATE_EXEC_DONE		3

class gate_exec : public djcallexec {
 public:
    gate_exec(ptr<catmgr> cmgr, djprot::call_reply_cb cb)
	: gc_(0), cb_(cb), cm_(cmgr)
    {
	pipes_[0] = pipes_[1] = -1;
    }

    virtual ~gate_exec() {
	if (gc_)
	    delete gc_;
	if (pipes_[0] >= 0) {
	    close(pipes_[0]);
	    fdcb(pipes_[0], selread, 0);
	}
	if (pipes_[1] >= 0)
	    close(pipes_[1]);
    }

    virtual void start(const dj_gatename &gate, const djcall_args &args) {
	try {
	    if (dj_label_debug) {
		label tl, tc;
		thread_cur_label(&tl);
		thread_cur_clearance(&tc);
		warn << "gate_exec::start: tl " << tl.to_string()
		     << ", tc " << tc.to_string() << "\n";
		warn << "gate_exec::start: grant " << args.grant.to_string()
		     << ", taint " << args.taint.to_string() << "\n";
	    }

	    cm_->acquire(args.grant, true);
	    cm_->acquire(args.taint, true);

	    proc_ct_ = start_env->proc_container;
	    gc_ = New gate_call(COBJ(gate.gate_ct, gate.gate_id),
				&args.taint, &args.grant, &args.taint);

	    dj_gate_call_outgoing(gc_->call_ct(),
				  gc_->call_grant(), gc_->call_taint(),
				  args.taint, args.grant, args.data,
				  &gcd_.param_obj, &call_vl_, &call_vc_);

	    errno_check(pipe(pipes_));
	    _make_async(pipes_[0]);
	    _make_async(pipes_[1]);

	    atomic_set(&state_, GATE_EXEC_CALL);
	    error_check(thread_create_option(start_env->proc_container,
					     &gate_exec::gate_call_thread, this, 0,
					     &callthread_tid_, "gate_exec call thread",
					     &callthread_args_, 0));

	    fdcb(pipes_[0], selread, wrap(mkref(this), &gate_exec::pipecb));
	} catch (std::exception &e) {
	    cprintf("gate_exec::start: %s\n", e.what());
	    cb_(REPLY_GATE_CALL_ERROR, (const djcall_args *) 0);
	}
    }

    virtual void abort() {
	if (atomic_compare_exchange(&state_, GATE_EXEC_CALL, GATE_EXEC_ABORT) == GATE_EXEC_CALL) {
	    /*
	     * Wait until the thread calls gate_enter().  Presumption that
	     * we are making a call into a different address space..
	     */
	    cobj_ref my_as;
	    sys_self_get_as(&my_as);
	    for (;;) {
		int r = sys_thread_trap(callthread_tid_, my_as, UTRAP_USER_NOP, 0);
		if (r == -E_INVAL)
		    break;

		cprintf("gate_exec::abort: slow gate_call_thread?\n");
		usleep(1000);
	    }

	    thread_cleanup(&callthread_args_);
	    delete gc_;
	    gc_ = 0;
	} else {
	    atomic_set(&state_, GATE_EXEC_ABORT);
	}

	cb_(REPLY_ABORTED, (const djcall_args *) 0);
    }

 private:
    static void gate_call_thread(void *arg) {
	gate_exec *ge = (gate_exec *) arg;
	ge->gc_->call(&ge->gcd_, &ge->call_vl_, &ge->call_vc_, &gate_exec::gate_return_cb, ge);

	if (ge->proc_ct_ != start_env->proc_container) {
	    cobj_ref save_taint_seg;
	    dj_gate_call_save_taint(ge->gcd_.taint_container,
				    &ge->gcd_, &save_taint_seg);

	    label tl, tc;
	    thread_cur_label(&tl);
	    tl.transform(label::star_to, tl.get_default());
	    ge->cm_->acquire(tl);

	    thread_cur_label(&tl);
	    thread_cur_clearance(&tc);

	    label gl, gc;
	    gate_compute_labels(ge->gc_->return_gate(), 0, &tl, &tc, &gl, &gc);

	    label vl(3), vc(0);
	    vl.set(start_env->process_grant, 0);
	    error_check(sys_self_set_verify(vl.to_ulabel(), vc.to_ulabel()));

	    gate_call_data *d = (gate_call_data *) tls_gate_args;
	    d->param_obj = save_taint_seg;
	    d->call_grant = start_env->process_grant;
	    gate_invoke(ge->gc_->return_gate(), &gl, &gc, 0, 0);
	    fatal << "gate_exec::gate_call_thread: untainted return call returned\n";
	}

	thread_cur_verify(&ge->reply_vl_, &ge->reply_vc_);
	ge->cm_->import(ge->reply_vl_, ge->gc_->call_grant(), ge->gc_->call_taint());
	atomic_compare_exchange(&ge->state_, GATE_EXEC_RETURN, GATE_EXEC_DONE);
	write(ge->pipes_[1], "", 1);
    }

    static void gate_return_cb(void *arg) {
	gate_exec *ge = (gate_exec *) arg;
	start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;

	sys_self_set_sched_parents(start_env_ro->proc_container, 0);
	if (atomic_compare_exchange(&ge->state_, GATE_EXEC_CALL, GATE_EXEC_RETURN) != GATE_EXEC_CALL) {
	    cprintf("gate_exec::gate_return_cb: aborted\n");
	    sys_self_halt();
	}
    }

    void pipecb() {
	char buf[64];
	read(pipes_[0], &buf[0], sizeof(buf));

	if (atomic_read(&state_) != GATE_EXEC_DONE)
	    return;
	fdcb(pipes_[0], selread, 0);

	try {
	    // If we did untainting, restore the saved state.
	    if (reply_vl_.get(start_env->process_grant) == 0)
		dj_gate_call_load_taint(gcd_.param_obj, &gcd_, &reply_vl_, &reply_vc_);

	    cm_->acquire(reply_vl_, true, gc_->call_grant(), gc_->call_taint());

	    djcall_args ra;
	    dj_gate_call_incoming(gcd_.param_obj, reply_vl_, reply_vc_,
				  gc_->call_grant(), gc_->call_taint(),
				  &ra.taint, &ra.grant, &ra.data);
	    cb_(REPLY_DONE, &ra);
	} catch (std::exception &e) {
	    cprintf("gate_exec::pipecb: %s\n", e.what());
	    cb_(REPLY_GATE_CALL_ERROR, (const djcall_args *) 0);
	}
    }

    gate_call *gc_;
    djprot::call_reply_cb cb_;
    gate_call_data gcd_;
    cobj_ref callthread_tid_;
    thread_args callthread_args_;
    label call_vl_, call_vc_;
    label reply_vl_, reply_vc_;
    atomic_t state_;
    int pipes_[2];
    ptr<catmgr> cm_;
    uint64_t proc_ct_;
};

ptr<djcallexec>
dj_gate_exec(ptr<catmgr> cmgr, djprot::call_reply_cb cb)
{
    return New refcounted<gate_exec>(cmgr, cb);
}
