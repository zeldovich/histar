#ifndef JOS_INC_GATESRV_HH
#define JOS_INC_GATESRV_HH

extern "C" {
#include <inc/gateparam.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>

class gatesrv_return {
 public:
    gatesrv_return(cobj_ref rgate, uint64_t tct, uint64_t gct,
		   void *stack, uint64_t flags)
	: rgate_(rgate), thread_ct_(tct), gate_tref_ct_(gct),
	  stack_(stack), flags_(flags) {}

    // ret will delete any labels passed to it
    void ret(label *owner, label *clear,
	     label *verify_owner = 0, label *verify_clear = 0)
	__attribute__((noreturn));

    void change_gate(cobj_ref newgate) { rgate_ = newgate; }
    static void cleanup_stub(label *owner, label *clear, void *arg);

 private:
    gatesrv_return(const gatesrv_return&);
    gatesrv_return &operator=(const gatesrv_return&);

    static void ret_tls_stub(uint64_t a0, uint64_t a1, uint64_t a2)
	__attribute__((noreturn));
    void ret_tls(label *owner, label *clear)
	__attribute__((noreturn));

    void cleanup(label *owner, label *clear);

    cobj_ref rgate_;
    uint64_t thread_ct_;
    uint64_t gate_tref_ct_;
    void *stack_;
    uint64_t flags_;
};

typedef void (*gatesrv_entry_t) (uint64_t, gate_call_data *, gatesrv_return *);

class gatesrv_descriptor {
 public:
    gatesrv_descriptor()
	: gate_container_(0), name_(0), as_(COBJ(0, 0)),
	  owner_(0), clear_(0), guard_(0),
	  func_(0), arg_(0), flags_(0) {};

    uint64_t gate_container_;
    const char *name_;

    cobj_ref as_;
    label *owner_;
    label *clear_;
    label *guard_;

    gatesrv_entry_t func_;
    uint64_t arg_;

    uint64_t flags_;

 private:
    gatesrv_descriptor(const gatesrv_descriptor&);
    gatesrv_descriptor &operator=(const gatesrv_descriptor&);
};

#define GATESRV_KEEP_TLS_STACK		0x0001
#define GATESRV_NO_THREAD_ADDREF	0x0002

cobj_ref gate_create(gatesrv_descriptor *dsc);
cobj_ref gate_create(uint64_t gate_container, const char *name,
		     label *owner, label *clear, label *guard,
		     gatesrv_entry_t func, uint64_t arg);

void gatesrv_entry_tls(uint64_t fn, uint64_t arg, uint64_t flags)
    __attribute__((noreturn));

#endif
