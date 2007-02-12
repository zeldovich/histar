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
    saved_privilege(uint64_t guard, uint64_t h);
    ~saved_privilege() { sys_obj_unref(gate_); }

    uint64_t handle() { return handle_; }
    void acquire();

private:
    uint64_t handle_;
    struct cobj_ref gate_;
};

class privilege_store {
public:
    privilege_store(uint64_t root_handle);
    ~privilege_store();

    uint64_t root_handle() { return root_handle_; }
    void store_priv(uint64_t h);
    void fetch_priv(uint64_t h);
    void drop_priv(uint64_t h);

private:
    uint64_t root_handle_;
    std::map<uint64_t, saved_privilege*> m_;
};

#endif
