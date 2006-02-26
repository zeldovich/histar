#ifndef JOS_INC_PRIVSTORE_HH
#define JOS_INC_PRIVSTORE_HH

// This is something like a *-handle pickler

#include <inc/gatesrv.hh>

class saved_privilege {
public:
    saved_privilege(uint64_t guard, uint64_t h);
    ~saved_privilege() { delete gate_; }

    uint64_t handle() { return handle_; }
    void acquire();

private:
    static void entry_stub(void *arg, struct cobj_ref p, gatesrv_return *r)
	__attribute__((noreturn));
    void entry(gatesrv_return *r) __attribute__((noreturn));

    uint64_t handle_;
    gatesrv *gate_;
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
    int slot_find(uint64_t h);
    int slot_alloc();

    uint64_t root_handle_;
    uint32_t privsize_;
    saved_privilege **privs_;
};

#endif
