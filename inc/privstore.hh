#ifndef JOS_INC_PRIVSTORE_HH
#define JOS_INC_PRIVSTORE_HH

// This is something like a *-handle pickler

extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
}

#include <inc/gatesrv.hh>
#include <map>

class saved_privilege {
 public:
    saved_privilege(uint64_t guard, uint64_t h, uint64_t ct);
    saved_privilege(uint64_t guard, uint64_t h, uint64_t h2, uint64_t ct);
    saved_privilege(uint64_t h, cobj_ref g) 
	: handle_(h), handle2_(0), gate_(g), gc_(false) {}
    saved_privilege(uint64_t h, uint64_t h2, cobj_ref g) 
	: handle_(h), handle2_(h2), gate_(g), gc_(false) {}

    ~saved_privilege() { if (gc_) sys_obj_unref(gate_); }

    uint64_t handle() { return handle_; }
    uint64_t handle2() { return handle2_; }
    cobj_ref gate() { return gate_; }
    void set_gc(bool b) { gc_ = b; }
    void acquire();

 private:
    saved_privilege(const saved_privilege&);
    saved_privilege &operator=(const saved_privilege&);
    void init(uint64_t guard, uint64_t h, uint64_t h2, uint64_t ct);

    uint64_t handle_;
    uint64_t handle2_;
    cobj_ref gate_;
    bool gc_;
};

class privilege_store {
 public:
    privilege_store(uint64_t root_handle);
    ~privilege_store();

    uint64_t root_handle() { return root_handle_; }
    void store_priv(uint64_t h);
    void fetch_priv(uint64_t h);
    void drop_priv(uint64_t h);
    bool has_priv(uint64_t h);

 private:
    privilege_store(const privilege_store&);
    privilege_store &operator=(const privilege_store&);

    uint64_t root_handle_;
    std::map<uint64_t, uint64_t> refcount_;
    std::map<uint64_t, saved_privilege*> m_;
};

#endif
