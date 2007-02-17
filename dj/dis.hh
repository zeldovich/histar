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
    vec<uint64_t> namedcats;
    label taint;
    label glabel;
    label gclear;
    str msg;

    dj_message_args() : send_timeout(0), taint(1), glabel(3), gclear(0) {}
};

typedef callback<void, const dj_message_args&, uint64_t>::ptr dj_msg_sink;

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

class djprot : virtual public refcount {
 public:
    typedef callback<void, dj_delivery_code, uint64_t>::ptr delivery_status_cb;
    typedef callback<void, const dj_message_endpoint&, const dj_message_args&,
			   delivery_status_cb>::ptr local_delivery_cb;

    virtual ~djprot() {}
    virtual str pubkey() const = 0;
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    virtual void send(str nodepk, const dj_message_endpoint &endpt,
		      const dj_message_args &msg, delivery_status_cb cb) = 0;
    virtual void set_delivery_cb(local_delivery_cb cb) = 0;
    virtual void set_catmgr(ptr<catmgr> cmgr) = 0;
    virtual ptr<catmgr> get_catmgr() = 0;

    static ptr<djprot> alloc(uint16_t port);
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
};

void dj_gate_delivery(ptr<catmgr> cmgr, const dj_message_endpoint&,
		      const dj_message_args&, djprot::delivery_status_cb);
void dj_debug_delivery(const dj_message_endpoint&, const dj_message_args&,
		       djprot::delivery_status_cb);

ptr<catmgr> dj_dummy_catmgr();
ptr<catmgr> dj_catmgr();
//ptr<djgate_incoming> dj_gate_incoming(ptr<djprot> p);

void dj_debug_sink(const dj_message_args&, uint64_t selftoken);

#endif
