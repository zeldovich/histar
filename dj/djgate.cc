#include <dj/dis.hh>

class gate_exec : public djcallexec {
 public:
    gate_exec(djprot::call_reply_cb cb) : cb_(cb) {}
    virtual ~gate_exec() {
	if (gc_)
	    delete gc_;
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

	    cobj_ref tid;
	    error_check(thread_create_option(start_env->proc_container,
					     &gate_exec::gate_call_thread, this,
					     &tid, "gate_exec call thread",
					     &callthread_, 0);
	} catch (std::exception &e) {
	    cprintf("gate_exec::start: %s\n", e.what());
	    cb_(REPLY_GATE_CALL_ERROR, (const djcall_args *) 0);
	}
    }

    virtual void abort() {
	cb_(REPLY_ABORTED, (const djcall_args *) 0);
    }

 private:
    static void gate_call_thread(void *arg) {
	gate_exec *ge = (gate_exec *) arg;
	ge->gc_->call(&gcd_, &grant_, return_cb, ge);
    }

    static void gate_return_cb(void *arg) {
	gate_exec *ge = (gate_exec *) arg;

	if (something) {
	    cprintf("gate_exec::gate_return_cb: halting..\n");
	    sys_self_halt();
	}
    }

    gate_call *gc_;
    djprot::call_reply_cb cb_;
    gate_call_data gcd_;
    thread_args callthread_;
    label grant_;
};

ptr<djcallexec>
dj_gate_exec(djprot::call_reply_cb cb)
{
    return New refcounted<gate_exec>(cb);
}
