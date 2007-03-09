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
	: rgate_(rgate), thread_ct_(tct), gate_tref_ct_(gct), stack_(stack), flags_(flags) {}

    // ret will delete the three labels passed to it
    void ret(label *contaminate_label,		// { 0 } for none
	     label *decontaminate_label,	// { 3 } for none
	     label *decontaminate_clearance,	// { 0 } for none
	     label *verify_label = 0,		// { 3 } for none
	     label *verify_clear = 0)		// { 0 } for none
	__attribute__((noreturn));

    void change_gate(cobj_ref newgate) { rgate_ = newgate; }
    static void cleanup_stub(label *tgt_s, label *tgt_r, void *arg);

 private:
    gatesrv_return(const gatesrv_return&);
    gatesrv_return &operator=(const gatesrv_return&);

    static void ret_tls_stub(gatesrv_return *r, label *tgt_label, label *tgt_clear)
	__attribute__((noreturn));
    void ret_tls(label *tgt_label, label *tgt_clear)
	__attribute__((noreturn));

    void cleanup(label *tgt_s, label *tgt_r);

    cobj_ref rgate_;
    uint64_t thread_ct_;
    uint64_t gate_tref_ct_;
    void *stack_;
    uint64_t flags_;
};

typedef void (*gatesrv_entry_t) (void *, gate_call_data *, gatesrv_return *);

class gatesrv_descriptor {
 public:
    gatesrv_descriptor()
	: gate_container_(0), name_(0), as_(COBJ(0, 0)),
	  label_(0), clearance_(0), verify_(0),
	  func_(0), arg_(0), flags_(0) {};

    uint64_t gate_container_;
    const char *name_;

    cobj_ref as_;
    label *label_;
    label *clearance_;
    label *verify_;

    gatesrv_entry_t func_;
    void *arg_;

    uint64_t flags_;

 private:
    gatesrv_descriptor(const gatesrv_descriptor&);
    gatesrv_descriptor &operator=(const gatesrv_descriptor&);
};

#define GATESRV_KEEP_TLS_STACK		0x01
#define GATESRV_NO_THREAD_ADDREF	0x02

cobj_ref gate_create(gatesrv_descriptor *dsc);
cobj_ref gate_create(uint64_t gate_container, const char *name,
		     label *label, label *clearance, label *verify,
		     gatesrv_entry_t func, void *arg);

#endif
