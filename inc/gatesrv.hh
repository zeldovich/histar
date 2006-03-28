#ifndef JOS_INC_GATESRV_HH
#define JOS_INC_GATESRV_HH

#include <inc/error.hh>
#include <inc/cpplabel.hh>

class gatesrv_return {
public:
    gatesrv_return(struct cobj_ref rgate, uint64_t tct, void *stack)
	: rgate_(rgate), thread_ct_(tct), stack_(stack) {}

    // ret will delete the three labels passed to it
    void ret(label *contaminate_label,		// { * } for none
	     label *decontaminate_label,	// { 3 } for none
	     label *decontaminate_clearance)	// { 0 } for none
	__attribute__((noreturn));

private:
    static void ret_tls_stub(gatesrv_return *r, label *tgt_label, label *tgt_clear)
	__attribute__((noreturn));
    void ret_tls(label *tgt_label, label *tgt_clear)
	__attribute__((noreturn));

    static void cleanup_stub(label *tgt_s, label *tgt_r, void *arg);
    void cleanup(label *tgt_s, label *tgt_r);

    struct cobj_ref rgate_;
    uint64_t thread_ct_;
    void *stack_;
};

typedef void (*gatesrv_entry_t)
	(void *, struct gate_call_data *, gatesrv_return *);

struct cobj_ref
    gate_create(uint64_t gate_container, const char *name,
		label *label, label *clearance,
		uint64_t entry_container,
		gatesrv_entry_t func, void *arg);

#endif
