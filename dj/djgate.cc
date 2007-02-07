extern "C" {
#include <inc/atomic.h>
#include <inc/utrap.h>
}

#include <dj/dis.hh>

#define GATE_EXEC_CALL		0
#define GATE_EXEC_RETURN	1
#define GATE_EXEC_ABORT		2
#define GATE_EXEC_DONE		3

class gate_exec : public djcallexec {
 public:
    gate_exec(djprot::call_reply_cb cb) : cb_(cb) {
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
	    grant_ = args.grant;
	    gc_ = New gate_call(COBJ(gate.gate_ct, gate.gate_id),
				&args.taint, &args.grant, &args.taint);

	    cobj_ref data_seg;
	    void *data_map = 0;
	    error_check(segment_alloc(gc_->call_ct(), args.data.size(),
				      &data_seg, &data_map,
				      args.taint.to_ulabel(), "gate_exec args"));
	    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
	    memcpy(data_map, args.data.base(), args.data.size());
	    gcd_.param_obj = args_seg;

	    errno_check(pipe(pipes_));
	    _make_async(pipes_[0]);
	    _make_async(pipes_[1]);

	    atomic_set(&state_, GATE_EXEC_CALL);
	    error_check(thread_create_option(start_env->proc_container,
					     &gate_exec::gate_call_thread, this,
					     &callthread_tid_ "gate_exec call thread",
					     &callthread_args_, 0);

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
	ge->gc_->call(&gcd_, &grant_, &gate_exec::gate_return_cb, ge);

	/*
	 * XXX
	 * might need to declassify ourselves at this point..
	 */

	atomic_compare_exchange(&ge->state_, GATE_EXEC_RETURN, GATE_EXEC_DONE);
	write(pipes_[1], "", 1);
    }

    static void gate_return_cb(void *arg) {
	gate_exec *ge = (gate_exec *) arg;
	start_env_t *start_env_ro = (start_env_t *) START_ENV_RO;

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
	    cobj_ref data_seg = gcd_.param_obj;

	    djcall_args ra;
	    thread_cur_verify(&ra.grant);
	    obj_get_label(data_seg, &ra.taint);

	    void *data_map = 0;
	    uint64_t data_len = 0;
	    error_check(segment_map(data_seg, 0, SEGMAP_READ,
				    &data_map, &data_len, 0));
	    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
	    ra.data = str(data_map, data_len);

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
    label call_grant_;
    label reply_grant_;
    atomic_t state_;
    int pipes_[2];
};

ptr<djcallexec>
dj_gate_exec(djprot::call_reply_cb cb)
{
    return New refcounted<gate_exec>(cb);
}
