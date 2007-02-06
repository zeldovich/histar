extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/memlayout.h>
}

#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static int label_debug = 0;

void
gate_compute_labels(struct cobj_ref gate,
		    label *cs, label *ds, label *dr,
		    label *tgt_label, label *tgt_clear)
{
    label tmp;

    // Compute the target label
    obj_get_label(gate, tgt_label);
    if (ds) {
	tgt_label->merge(ds, &tmp, label::min, label::leq_starlo);
	tgt_label->copy_from(&tmp);
    }

    label thread_label;
    thread_cur_label(&thread_label);
    thread_label.transform(label::star_to, 0);
    tgt_label->merge(&thread_label, &tmp, label::max, label::leq_starhi);
    tgt_label->copy_from(&tmp);

    if (cs) {
	tgt_label->merge(cs, &tmp, label::max, label::leq_starlo);
	tgt_label->copy_from(&tmp);
    }

    // Compute the target clearance
    gate_get_clearance(gate, tgt_clear);
    if (dr) {
	tgt_clear->merge(dr, &tmp, label::max, label::leq_starlo);
	tgt_clear->copy_from(&tmp);
    }

    // For any star levels in tgt_label & thread_label, grant a 3 in tgt_clear
    {
	thread_cur_label(&thread_label);
	label common_star3;

	tgt_label->merge(&thread_label, &common_star3, label::max, label::leq_starlo);
	common_star3.transform(label::nonstar_to, 0);
	common_star3.transform(label::star_to, 3);

	tgt_clear->merge(&common_star3, &tmp, label::max, label::leq_starlo);
	tgt_clear->copy_from(&tmp);
    }

    if (label_debug) {
	cprintf("gate_compute_labels: cs %s ds %s dr %s\n",
		cs ? cs->to_string() : "null",
		ds ? ds->to_string() : "null",
		dr ? dr->to_string() : "null");
	cprintf("gate_compute_labels: tgt label %s clearance %s\n",
		tgt_label->to_string(), tgt_clear->to_string());
    }
}

static void __attribute__((noinline)) __attribute__((noreturn))
gate_invoke2(struct cobj_ref gate, label *tgt_label, label *tgt_clear,
	     gate_invoke_cb cb, void *arg, uint64_t lsize, uint64_t csize)
{
    uint64_t tgt_label_ent[lsize];
    uint64_t tgt_clear_ent[csize];

    label tgt_label_stack(&tgt_label_ent[0], lsize);
    label tgt_clear_stack(&tgt_clear_ent[0], csize);

    try {
	tgt_label_stack.copy_from(tgt_label);
	tgt_clear_stack.copy_from(tgt_clear);
    } catch (std::exception &e) {
	cprintf("gate_invoke: cannot copy return labels: label %s, clear %s\n",
		tgt_label->to_string(), tgt_clear->to_string());
	throw;
    }

    if (cb)
	cb(tgt_label, tgt_clear, arg);

    if (label_debug)
	cprintf("gate_invoke: label %s, clearance %s\n",
		tgt_label_stack.to_string(), tgt_clear_stack.to_string());

    error_check(sys_gate_enter(gate,
			       tgt_label_stack.to_ulabel(),
			       tgt_clear_stack.to_ulabel(), 0));
    throw basic_exception("gate_invoke: still alive");
}

void
gate_invoke(struct cobj_ref gate, label *tgt_label, label *tgt_clear,
	    gate_invoke_cb cb, void *arg)
{
    uint64_t lents = tgt_label->to_ulabel()->ul_nent;
    uint64_t cents = tgt_clear->to_ulabel()->ul_nent;

    uint64_t lbytes = (lents + cents) * 8;
    if (lbytes > 512) {
	uint64_t tlsbytes = PGSIZE + lbytes;
	if (label_debug)
	    cprintf("[%ld] gate_invoke: growing TLS to %ld bytes\n",
		    thread_id(), tlsbytes);
	error_check(sys_segment_resize(COBJ(0, kobject_id_thread_sg), tlsbytes));
	if (label_debug)
	    cprintf("[%ld] gate_invoke: growing TLS to %ld bytes OK\n",
		    thread_id(), tlsbytes);
    }

    gate_invoke2(gate, tgt_label, tgt_clear, cb, arg, lents, cents);
}
