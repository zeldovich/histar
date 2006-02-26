extern "C" {
#include <inc/syscall.h>
}

#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>

static int label_debug = 1;

void
gate_invoke(struct cobj_ref gate, label *cs, label *ds, label *dr,
	    gate_invoke_cb cb, void *arg)
{
    struct cobj_ref thread_self = COBJ(kobject_id_thread_ct, thread_id());

    enum { label_buf_size = 48 };
    uint64_t tgt_label_ent[label_buf_size];
    uint64_t tgt_clear_ent[label_buf_size];
    uint64_t tmp_ent[label_buf_size];

    label tgt_label(&tgt_label_ent[0], label_buf_size);
    label tgt_clear(&tgt_clear_ent[0], label_buf_size);
    label tmp(&tmp_ent[0], label_buf_size);

    // Compute the target label
    error_check(sys_obj_get_label(gate, tgt_label.to_ulabel()));
    tgt_label.merge_with(ds, label::min, label::leq_starlo);

    error_check(sys_obj_get_label(thread_self, tmp.to_ulabel()));
    tmp.transform(label::star_to_0);
    tgt_label.merge_with(&tmp, label::max, label::leq_starhi);
    tgt_label.merge_with(cs, label::max, label::leq_starlo);

    // Compute the target clearance
    error_check(sys_gate_clearance(gate, tgt_clear.to_ulabel()));
    tgt_clear.merge_with(dr, label::max, label::leq_starlo);

    if (cb)
	cb(cs, ds, dr, arg);

    if (label_debug)
	cprintf("gate_invoke: label %s, clearance %s\n",
		tgt_label.to_string(), tgt_clear.to_string());

    error_check(sys_gate_enter(gate,
			       tgt_label.to_ulabel(),
			       tgt_clear.to_ulabel(), 0, 0));
    throw basic_exception("gate_invoke: still alive");
}
