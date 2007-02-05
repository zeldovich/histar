extern "C" {
#include <inc/lib.h>
#include <inc/assert.h>

#include <inc/stdio.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/gateinvoke.hh>
#include <inc/ssldclnt.hh>

struct worker_args {
    struct cobj_ref cow_gate;
    struct cobj_ref eproc_biseg;
    uint64_t root_ct;
    uint64_t taint;
};

static void __attribute__((noreturn))
eproc_worker_setup(void *b)
{
    struct worker_args *a = (struct worker_args *)b;
        
    struct cobj_ref cow_gate = a->cow_gate;
    uint64_t taint = a->taint;

    struct ssl_eproc_cow_args *d = (struct ssl_eproc_cow_args *)tls_gate_args;
    d->privkey_biseg = a->eproc_biseg;
    d->root_ct = a->root_ct;

    label tgt_label, tgt_clear;
    obj_get_label(cow_gate, &tgt_label);
    gate_get_clearance(cow_gate, &tgt_clear);
    tgt_label.set(taint, 3);
    tgt_clear.set(taint, 3);

    try {
	gate_invoke(cow_gate, &tgt_label, &tgt_clear, 0, 0);
    } catch (std::exception &e) {
	panic("eproc_worker_setup: could not invoke cow_gate: %s", e.what());
    }
}

void
ssl_eproc_taint_cow(struct cobj_ref gate, struct cobj_ref eproc_seg, 
		    uint64_t root_ct, uint64_t taint)
{
    struct worker_args a;
    a.cow_gate = gate;
    a.eproc_biseg = eproc_seg;
    a.root_ct = root_ct;
    a.taint = taint;

    struct cobj_ref t;
    int r = thread_create_option(root_ct, &eproc_worker_setup,
				 &a, sizeof(a), 
				 &t, "eproc-worker", 0, THREAD_OPT_ARGCOPY);
    error_check(r);    
}
