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

struct worker_args {
    cobj_ref cow_gate;
    jcomm_ref cipher_comm;
    jcomm_ref plain_comm;
    jcomm_ref eproc_comm;
    uint64_t root_ct;
    uint64_t taint;
};

static void __attribute__((noreturn))
ssld_worker_setup(void *b)
{
    struct worker_args *a = (struct worker_args *)b;
        
    struct cobj_ref cow_gate = a->cow_gate;
    uint64_t taint = a->taint;

    struct ssld_cow_args *d =
	(struct ssld_cow_args *) &tls_data->tls_gate_args.param_buf[0];
    memset(d, 0, sizeof(*d));
    d->cipher_comm = a->cipher_comm;
    d->plain_comm = a->plain_comm;
    d->root_ct = a->root_ct;
    d->privkey_comm = a->eproc_comm;

    uint64_t tgt_label_ent[16];
    uint64_t tgt_clear_ent[16];

    label tgt_label(&tgt_label_ent[0], 16);
    label tgt_clear(&tgt_clear_ent[0], 16);

    obj_get_label(cow_gate, &tgt_label);
    gate_get_clearance(cow_gate, &tgt_clear);
    tgt_label.set(taint, 3);
    tgt_clear.set(taint, 3);

    try {
	gate_invoke(cow_gate, &tgt_label, &tgt_clear, 0, 0);
    } catch (std::exception &e) {
	panic("ssld_worker_setup: could not invoke cow_gate: %s", e.what());
    }
}

void
ssld_taint_cow(cobj_ref cow_gate, jcomm_ref eproc_comm,
	       jcomm_ref cipher_comm, jcomm_ref plain_comm,
	       uint64_t root_ct, uint64_t taint, thread_args *ta)
{
    struct worker_args a;    
    a.cipher_comm = cipher_comm;
    a.plain_comm = plain_comm;
    a.cow_gate = cow_gate;
    a.eproc_comm = eproc_comm;
    a.root_ct = root_ct;
    a.taint = taint;

    struct cobj_ref t;
    int r = thread_create_option(root_ct, &ssld_worker_setup,
				 &a, sizeof(a), 
				 &t, "ssld-worker", ta, THREAD_OPT_ARGCOPY);
    error_check(r);    
}
