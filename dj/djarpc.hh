#ifndef JOS_DJ_DJARPC_HH
#define JOS_DJ_DJARPC_HH

#include <dj/djprot.hh>
#include <dj/stuff.hh>
#include <dj/djrpc.hh>

class dj_arpc_call : virtual public refcount {
 public:
    typedef callback<void, dj_delivery_code, const dj_message*>::ptr call_reply_cb;

    dj_arpc_call(message_sender *s, dj_gate_factory *f, uint64_t rct)
	: s_(s), f_(f), rct_(rct), rep_created_(false), done_(false) {}
    ~dj_arpc_call();
    void call(const dj_pubkey&, time_t tmo, const dj_delegation_set&,
	      const dj_message&, const str&, call_reply_cb cb,
	      const dj_catmap *return_cm = 0,
	      const dj_delegation_set *return_ds = 0);

 private:
    void retransmit();
    void delivery_cb(dj_delivery_code);
    void reply_sink(const dj_pubkey&, const dj_message&);

    message_sender *s_;
    dj_gate_factory *f_;

    uint64_t rct_;
    bool rep_created_;
    dj_message_endpoint rep_;

    dj_pubkey dst_;

    dj_delegation_set dset_;
    dj_message a_;
    call_reply_cb cb_;

    bool done_;
    time_t until_;
};

typedef callback<void, bool, const dj_rpc_reply&>::ptr dj_arpc_cb;

struct dj_arpc_reply {
    dj_rpc_reply r;
    dj_arpc_cb cb;
};

typedef callback<void, const dj_message&, const str&,
		       const dj_arpc_reply&>::ptr dj_arpc_service;

void dj_arpc_srv_sink(message_sender*, dj_arpc_service,
		      const dj_pubkey&, const dj_message&);

// Convert a threaded RPC service to a crude asynchronous RPC service.
void dj_rpc_to_arpc(dj_rpc_service_cb, const dj_message&,
		    const str&, const dj_arpc_reply&);

#endif
