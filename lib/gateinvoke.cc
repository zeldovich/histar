extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/memlayout.h>

#include <inttypes.h>
}

#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static int label_debug = 0;

static void __attribute__((noinline)) __attribute__((noreturn))
gate_invoke2(struct cobj_ref gate, label *owner, label *clear,
	     gate_invoke_cb cb, void *arg, uint64_t osize, uint64_t csize)
{
    uint64_t owner_ent[osize];
    uint64_t clear_ent[csize];

    label owner_stack(&owner_ent[0], osize);
    label clear_stack(&clear_ent[0], csize);

    try {
	owner_stack = *owner;
	clear_stack = *clear;
    } catch (std::exception &e) {
	cprintf("gate_invoke: cannot copy return labels: owner %s, clear %s\n",
		owner->to_string(), clear->to_string());
	throw;
    }

    if (cb)
	cb(owner, clear, arg);

    if (label_debug)
	cprintf("gate_invoke: owner %s, clear %s\n",
		owner_stack.to_string(), clear_stack.to_string());

    error_check(sys_gate_enter(gate,
			       owner_stack.to_ulabel(),
			       clear_stack.to_ulabel(), 0));
    throw basic_exception("gate_invoke: still alive");
}

void
gate_invoke(struct cobj_ref gate, label *owner, label *clear,
	    gate_invoke_cb cb, void *arg)
{
    uint64_t oents = owner->to_ulabel()->ul_nent;
    uint64_t cents = clear->to_ulabel()->ul_nent;

    uint64_t lbytes = (oents + cents) * 8;
    if (lbytes > 512) {
	uint64_t tlsbytes = UTLS_DEFSIZE + lbytes;
	if (label_debug)
	    cprintf("[%"PRIu64"] gate_invoke: growing TLS to %"PRIu64" bytes\n",
		    thread_id(), tlsbytes);
	error_check(sys_segment_resize(COBJ(0, kobject_id_thread_sg), tlsbytes));
	if (label_debug)
	    cprintf("[%"PRIu64"] gate_invoke: growing TLS to %"PRIu64" bytes OK\n",
		    thread_id(), tlsbytes);
    }

    gate_invoke2(gate, owner, clear, cb, arg, oents, cents);
}
