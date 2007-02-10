#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>
#include <async.h>
#include <dj/dj.h>

struct djcall_args {
    str data;
    label taint;
    label grant;
};

typedef callback<bool, const djcall_args&, djcall_args*>::ptr djgate_service_cb;

class djcallexec : virtual public refcount {
 public:
    virtual ~djcallexec() {}
    virtual void start(const dj_gatename &gate, const djcall_args &args) = 0;
    virtual void abort() = 0;
};

class catmgr : virtual public refcount {
 public:
    virtual ~catmgr() {}
    virtual uint64_t alloc() = 0;
    virtual void release(uint64_t c) = 0;
};

class djprot : virtual public refcount {
 public:
    typedef callback<void, dj_reply_status, const djcall_args*>::ptr call_reply_cb;
    typedef callback<ptr<djcallexec>, call_reply_cb>::ptr callexec_factory;

    virtual ~djprot() {}
    virtual str pubkey() const = 0;
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    virtual void call(str nodepk, const dj_gatename &gate,
		      const djcall_args &args, call_reply_cb cb) = 0;
    virtual void set_callexec(callexec_factory cb) = 0;
    virtual void set_catmgr(ptr<catmgr> cmgr) = 0;

    static ptr<djprot> alloc(uint16_t port);
};

class djgate_incoming : virtual public refcount {
 public:
    virtual ~djgate_incoming() {}
    virtual cobj_ref gate() = 0;
};

ptr<djcallexec> dj_gate_exec(djprot::call_reply_cb);
bool dj_echo_service(const djcall_args &in, djcall_args *out);
bool dj_posixfs_service(const djcall_args &in, djcall_args *out);

ptr<catmgr> dj_dummy_catmgr();
ptr<djgate_incoming> dj_gate_incoming(ptr<djprot> p);

#endif
