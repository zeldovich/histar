#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>
#include <async.h>
#include <dj/dj.h>

enum { dj_label_debug = 0 };

struct dj_message_args {
    uint32_t send_timeout;	/* seconds */

    dj_esign_pubkey sender;
    uint64_t msg_ct;
    uint64_t token;
    label taint;
    label glabel;
    label gclear;
    str msg;

    dj_message_args() : send_timeout(0), taint(1), glabel(3), gclear(0) {}
};

typedef callback<void, const dj_message_args&, uint64_t>::ptr dj_msg_sink;
typedef callback<void, dj_delivery_code, uint64_t>::ptr delivery_status_cb;

class message_sender {
 public:
    virtual ~message_sender() {}
    virtual void send(const dj_esign_pubkey &node, const dj_message_endpoint &ep,
		      const dj_message_args &msg, delivery_status_cb cb) = 0;
};

class catmgr : virtual public refcount {
 public:
    virtual ~catmgr() {}
    virtual uint64_t alloc() = 0;
    virtual void release(uint64_t c) = 0;
    virtual void acquire(const label &l, bool droplater = false,
			 uint64_t except0 = 0, uint64_t except1 = 0) = 0;
    virtual void import(const label &l, uint64_t except0 = 0,
					uint64_t except1 = 0) = 0;
};

class djprot : public message_sender {
 public:
    typedef callback<void, const dj_message_endpoint&, const dj_message_args&,
			   delivery_status_cb>::ptr local_delivery_cb;

    virtual ~djprot() {}
    virtual dj_esign_pubkey pubkey() const = 0;
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    virtual void set_delivery_cb(local_delivery_cb cb) = 0;
    virtual void set_catmgr(ptr<catmgr> cmgr) = 0;
    virtual ptr<catmgr> get_catmgr() = 0;

    static djprot *alloc(uint16_t port);
};

class dj_incoming_gate {
 public:
    virtual ~dj_incoming_gate() {}
    virtual cobj_ref gate() = 0;
};

class dj_gate_factory {
 public:
    virtual ~dj_gate_factory() {}
    virtual dj_message_endpoint create_gate(uint64_t ct, dj_msg_sink) = 0;
    virtual void destroy(const dj_message_endpoint&) = 0;
};

class dj_rpc_call {
 public:
    typedef callback<void, dj_delivery_code, const dj_message_args*>::ptr call_reply_cb;

    dj_rpc_call(message_sender *s, dj_gate_factory *f, uint64_t rct)
	: s_(s), f_(f), rct_(rct), rep_created_(false), reply_token_(0) {}
    ~dj_rpc_call();
    void call(const dj_esign_pubkey &node, const dj_message_endpoint &ep,
	      dj_message_args&, const str&, call_reply_cb cb);

 private:
    void delivery_cb(dj_delivery_code, uint64_t token);
    void reply_sink(const dj_message_args&, uint64_t token);

    message_sender *s_;
    dj_gate_factory *f_;
    uint64_t rct_;
    call_reply_cb cb_;
    bool rep_created_;
    dj_esign_pubkey dst_;
    uint64_t reply_token_;
    dj_message_endpoint rep_;
    dj_message_args a_;
};

void dj_gate_delivery(ptr<catmgr> cmgr, const dj_message_endpoint&,
		      const dj_message_args&, delivery_status_cb);
void dj_debug_delivery(const dj_message_endpoint&, const dj_message_args&,
		       delivery_status_cb);

ptr<catmgr> dj_dummy_catmgr();
ptr<catmgr> dj_catmgr();
//ptr<djgate_incoming> dj_gate_incoming(djprot *p);

typedef callback<void, const dj_message_args&, const str&, dj_message_args*>::ptr dj_call_service;
void dj_echo_service(const dj_message_args&, const str&, dj_message_args*);

void dj_debug_sink(const dj_message_args&, uint64_t selftoken);
void dj_rpc_call_sink(message_sender*, dj_call_service, const dj_message_args&, uint64_t selftoken);

#endif
