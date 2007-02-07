#include <dj/directexec.hh>

class direct_exec : public djcallexec {
 public:
    direct_exec(dj_direct_gatemap *m, djprot::call_reply_cb cb)
	: m_(m), cb_(cb) {}

    virtual void start(const dj_gatename &gate, const djcall_args &args) {
	djgate_service_cb *srv = m_->gatemap_[COBJ(gate.gate_ct, gate.gate_id)];
	if (!srv) {
	    cb_(REPLY_GATE_CALL_ERROR, (const djcall_args *) 0);
	    return;
	}

	djcall_args out;
	if (!((*srv)(args, &out))) {
	    cb_(REPLY_GATE_CALL_ERROR, (const djcall_args *) 0);
	    return;
	}

	cb_(REPLY_DONE, &out);
    }

    virtual void abort() {
	cb_(REPLY_ABORTED, (const djcall_args *) 0);
    }

 private:
    dj_direct_gatemap *m_;
    djprot::call_reply_cb cb_;
};

ptr<djcallexec>
dj_direct_gatemap::newexec(djprot::call_reply_cb cb)
{
    return New refcounted<direct_exec>(this, cb);
}

bool
dj_echo_service(const djcall_args &in, djcall_args *out)
{
    warn << "dj_echo_service: data " << in.data << "\n";
    *out = in;
    return true;
}
