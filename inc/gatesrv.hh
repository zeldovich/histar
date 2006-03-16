#ifndef JOS_INC_GATESRV_HH
#define JOS_INC_GATESRV_HH

#include <inc/error.hh>
#include <inc/cpplabel.hh>

class gatesrv_return {
public:
    gatesrv_return(struct cobj_ref rgate, uint64_t tct, void *tls, void *stack)
	: rgate_(rgate), thread_ct_(tct), tls_(tls), stack_(stack) {}

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
    void *tls_;
    void *stack_;
};

typedef void (*gatesrv_entry_t)
	(void *, struct gate_call_data *, gatesrv_return *);

class gatesrv {
public:
    gatesrv(uint64_t gate_container, const char *name,
	    label *label, label *clearance);
    ~gatesrv();

    // Entry container used for thread reference and entry stack
    void set_entry_container(uint64_t ct) { entry_container_ = ct; }
    void set_entry_function(gatesrv_entry_t f, void *arg) { f_ = f; arg_ = arg; }
    void enable() { active_ = 1; }

    struct cobj_ref gate() { return gate_obj_; }

private:
    static void entry_tls_stub(gatesrv *s) __attribute__((noreturn));
    void entry_tls() __attribute__((noreturn));
    static void entry_stub(gatesrv *s, void *stack) __attribute__((noreturn));
    void entry(void *stack) __attribute__((noreturn));

    struct cobj_ref gate_obj_;
    void *tls_;		// thread-local-segment
    uint32_t stackpages_;

    uint64_t entry_container_;
    gatesrv_entry_t f_;
    void *arg_;
    int active_;
};

#endif
