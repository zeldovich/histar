extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/fd.h>
#include <inc/netd.h>
#include <inc/bipipe.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/spawn.hh>
#include <inc/ssldclnt.hh>
#include <inc/labelutil.hh>
#include <inc/gateinvoke.hh>

static void __attribute__((noreturn))
ssld_worker_setup(void *b)
{
    // XXX
    struct args {
	struct cobj_ref cipher_biseg;
	struct cobj_ref plain_biseg;
	struct cobj_ref cow_gate;
	uint64_t root_ct;
	uint64_t taint;
    } *a = (struct args *)b;
    
    struct cobj_ref cow_gate = a->cow_gate;
    uint64_t taint = a->taint;

    struct ssld_cow_args *d = (struct ssld_cow_args *)tls_gate_args;
    d->cipher_biseg = a->cipher_biseg;
    d->plain_biseg = a->plain_biseg;
    d->root_ct = a->root_ct;

    free(a);    
    
    label tgt_label, tgt_clear;
    obj_get_label(cow_gate, &tgt_label);
    gate_get_clearance(cow_gate, &tgt_clear);
    tgt_label.set(taint, 3);
    tgt_clear.set(taint, 3);

    gate_invoke(cow_gate, &tgt_label, &tgt_clear, 0, 0);
    
}

void
ssld_taint_cow(struct cobj_ref cow_gate,
	       struct cobj_ref cipher_biseg, struct cobj_ref plain_biseg,
	       uint64_t root_ct, uint64_t taint,
	       thread_args *ta)
{
    // XXX
    struct args {
	struct cobj_ref cipher_biseg;
	struct cobj_ref plain_biseg;
	struct cobj_ref cow_gate;
	uint64_t root_ct;
	uint64_t taint;
    } *a = (struct args *)malloc(sizeof(*a));
    a->cipher_biseg = cipher_biseg;
    a->plain_biseg = plain_biseg;
    a->cow_gate = cow_gate;
    a->root_ct = root_ct;
    a->taint = taint;

    struct cobj_ref t;
    int r = thread_create_option(root_ct, &ssld_worker_setup,
				 a, 0, &t, "ssld-worker", ta, 0);
    error_check(r);

    
    
}
