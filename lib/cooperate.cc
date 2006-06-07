extern "C" {
#include <inc/syscall.h>
#include <inc/cooperate.h>
#include <inc/memlayout.h>
#include <inc/gateparam.h>
#include <inc/assert.h>
#include <inc/error.h>

#include <string.h>
}

#include <inc/cooperate.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/gateinvoke.hh>

extern char cooperate_syscall[], cooperate_syscall_end[];

#define should_be(expr)						\
    do {							\
	if (!(expr))						\
	    throw basic_exception("Mismatch: %s", #expr);	\
    } while (0)

enum {
    coop_as_slots = 3
};

static void
coop_gen_as(cobj_ref status_seg, cobj_ref text_seg, uint64_t dseg_len,
	    struct u_address_space *uas)
{
    struct u_segment_mapping usm[coop_as_slots] = {
	{ status_seg, 0, 1, 0,
	  SEGMAP_READ | SEGMAP_WRITE, (void *) COOP_STATUS },
	{ text_seg,   0, ROUNDUP(dseg_len, PGSIZE) / PGSIZE, 1,
	  SEGMAP_READ | SEGMAP_EXEC,  (void *) COOP_TEXT },
	{ COBJ(0, kobject_id_thread_sg), 0, 1, 2,
	  SEGMAP_READ | SEGMAP_WRITE, (void *) UTLS },
	};

    if (uas->size < coop_as_slots)
	throw error(-E_NO_SPACE, "address space buffer too small");

    uas->nent = coop_as_slots;
    memcpy(uas->ents, usm, sizeof(usm));
}

cobj_ref
coop_gate_create(uint64_t container,
		 label *l,
		 label *clearance,
		 coop_sysarg arg_values[8],
		 char arg_freemask[8])
{
    // First, compute the space needed in the text/data segment.
    uint64_t data_seg_len = 0;

    // Code length.
    uint64_t code_len = (cooperate_syscall_end - cooperate_syscall);
    data_seg_len += code_len;

    // System call argument pointers and fixed-value arguments.
    uint64_t coop_syscall_args_offset = data_seg_len;
    data_seg_len += sizeof(struct coop_syscall_args);

    uint64_t coop_syscall_argval_offset = data_seg_len;
    data_seg_len += sizeof(struct coop_syscall_argval);

    uint64_t coop_brk_offset = data_seg_len;

    // Any labels we need to pass in.
    for (int i = 0; i < 8; i++) {
	if (arg_values[i].is_label && arg_values[i].u.l) {
	    if (arg_freemask[i])
		throw basic_exception("Unbound label args not supported");

	    data_seg_len += sizeof(struct ulabel);
	    data_seg_len += arg_values[i].u.l->to_ulabel()->ul_nent *
			    sizeof(arg_values[i].u.l->to_ulabel()->ul_ent[0]);
	}
    }

    // Allocate the status and text/data segments.
    cobj_ref status_seg, text_seg;
    char *text_va = 0;

    error_check(segment_alloc(container, sizeof(struct coop_status),
			      &status_seg, 0, 0, "coop status"));
    scope_guard<int, cobj_ref> status_unref(sys_obj_unref, status_seg);

    error_check(segment_alloc(container, data_seg_len,
			      &text_seg, (void **) &text_va, 0, "coop text"));
    scope_guard<int, cobj_ref> text_unref(sys_obj_unref, text_seg);
    scope_guard<int, void *> text_unmap(segment_unmap, text_va);

    // Fill in the text segment
    memcpy(text_va, cooperate_syscall, code_len);

    struct coop_syscall_args *csa_ptr =
	(struct coop_syscall_args *) (text_va + coop_syscall_args_offset);
    struct coop_syscall_argval *csa_val =
	(struct coop_syscall_argval *) (text_va + coop_syscall_argval_offset);

    struct gate_call_data *tls_args =
	(struct gate_call_data *) TLS_GATE_ARGS;
    struct coop_syscall_argval *csa_free =
	(struct coop_syscall_argval *) &tls_args->param_buf[0];

    for (int i = 0; i < 8; i++) {
	if (arg_freemask[i]) {
	    csa_ptr->args[i] = &csa_free->argval[i];
	} else {
	    csa_ptr->args[i] = (uint64_t *) (COOP_TEXT + coop_syscall_argval_offset + i * 8);
	    if (!(arg_values[i].is_label && arg_values[i].u.l)) {
		csa_val->argval[i] = arg_values[i].u.i;
	    } else {
		ulabel *ul = arg_values[i].u.l->to_ulabel();

		ulabel *ul_copy = (ulabel *) (text_va + coop_brk_offset);
		coop_brk_offset += sizeof(*ul);

		ul_copy->ul_ent = (uint64_t *) (COOP_TEXT + coop_brk_offset);
		uint64_t *copy_ents = (uint64_t *) (text_va + coop_brk_offset);
		coop_brk_offset += ul->ul_nent * sizeof(uint64_t);

		ul_copy->ul_nent = ul->ul_nent;
		ul_copy->ul_default = ul->ul_default;
		memcpy(copy_ents, ul->ul_ent, ul->ul_nent * sizeof(uint64_t));

		csa_val->argval[i] = (((char *) ul_copy) - text_va) + COOP_TEXT;
	    }
	}
    }

    assert(coop_brk_offset == data_seg_len);

    // Allocate an address space and fill it in.
    int64_t as_id = sys_as_create(container, 0, "coop as");
    error_check(as_id);

    cobj_ref as = COBJ(container, as_id);
    scope_guard<int, cobj_ref> as_unref(sys_obj_unref, as);

    struct u_segment_mapping usm[coop_as_slots];
    struct u_address_space uas = { coop_as_slots, coop_as_slots, &usm[0] };
    coop_gen_as(status_seg, text_seg, data_seg_len, &uas);
    error_check(sys_as_set(as, &uas));

    // Mark AS & text segment read-only.
    error_check(sys_obj_set_readonly(as));
    error_check(sys_obj_set_readonly(text_seg));

    // Create a gate..
    struct thread_entry te;
    memset(&te, 0, sizeof(te));

    te.te_as = as;
    te.te_entry = (void *) COOP_TEXT;
    te.te_arg[0] = COOP_TEXT + coop_syscall_args_offset;

    int64_t gate_id =
	sys_gate_create(container, &te,
			clearance->to_ulabel(), l->to_ulabel(),
			"coop gate", 1);
    error_check(gate_id);

    // We're done!
    status_unref.dismiss();
    text_unref.dismiss();
    as_unref.dismiss();

    return COBJ(container, gate_id);
}

static void
coop_verify(cobj_ref coop_gate, coop_sysarg arg_values[8],
	    cobj_ref *status_segp)
{
    int64_t r;
    uint64_t code_len = (cooperate_syscall_end - cooperate_syscall);

    struct thread_entry te;
    error_check(sys_gate_get_entry(coop_gate, &te));

    should_be(te.te_entry == (void *) COOP_TEXT);
    should_be(te.te_arg[0] == COOP_TEXT + code_len);

    error_check(r = sys_obj_get_readonly(te.te_as));
    should_be(r == 1);

    struct u_segment_mapping usm_actual[coop_as_slots];
    struct u_address_space uas_actual = { coop_as_slots, coop_as_slots, &usm_actual[0] };

    struct u_segment_mapping usm_ideal[coop_as_slots];
    struct u_address_space uas_ideal = { coop_as_slots, coop_as_slots, &usm_ideal[0] };

    error_check(sys_as_get(te.te_as, &uas_actual));
    should_be(uas_actual.nent == coop_as_slots);

    cobj_ref status_seg = uas_actual.ents[0].segment;
    cobj_ref data_seg = uas_actual.ents[1].segment;

    error_check(r = sys_obj_get_readonly(data_seg));
    should_be(r == 1);

    error_check(r = sys_segment_get_nbytes(data_seg));
    uint64_t dseg_len = r;

    coop_gen_as(status_seg, data_seg, dseg_len, &uas_ideal);
    should_be(!memcmp(&usm_actual[0], &usm_ideal[0], sizeof(usm_ideal)));

    // === Address space is verified OK; now verify text.

    // First we copy the segment to ensure it's not unref'ed under us.
    int64_t copy_id;
    error_check(copy_id = sys_segment_copy(data_seg, start_env->proc_container,
					   0, "coop verify text"));

    cobj_ref data_seg_copy = COBJ(start_env->proc_container, copy_id);
    scope_guard<int, cobj_ref> copy_unref(sys_obj_unref, data_seg_copy);

    char *text_va = 0;
    error_check(segment_map(data_seg_copy, 0, SEGMAP_READ,
			    (void **) &text_va, &dseg_len, 0));
    scope_guard<int, void *> unmap(segment_unmap, text_va);

    should_be(!memcmp(text_va, &cooperate_syscall, code_len));

    // === Text OK; now verify arguments.

    struct coop_syscall_args *csa_ptr =
	(struct coop_syscall_args *) (text_va + code_len);
    struct coop_syscall_argval *csa_val =
	(struct coop_syscall_argval *) (text_va + code_len + sizeof(*csa_ptr));

    struct gate_call_data *tls_args =
	(struct gate_call_data *) TLS_GATE_ARGS;
    struct coop_syscall_argval *csa_free =
	(struct coop_syscall_argval *) &tls_args->param_buf[0];

    uint64_t brk_offset = code_len + sizeof(*csa_ptr) + sizeof(*csa_val);
    should_be(brk_offset <= dseg_len);

    for (int i = 0; i < 8; i++) {
	uint64_t *aptr = csa_ptr->args[i];

	if (aptr == &csa_free->argval[i]) {
	    if (arg_values[i].is_label)
		throw basic_exception("Unbound label args not supported");
	    continue;
	}

	should_be(aptr == (uint64_t *)
		    (COOP_TEXT + code_len + sizeof(*csa_ptr) + i * 8));
	uint64_t aval = csa_val->argval[i];

	if (!arg_values[i].is_label) {
	    should_be(aval == arg_values[i].u.i);
	} else {
	    ulabel *ul = arg_values[i].u.l->to_ulabel();

	    should_be(aval == COOP_TEXT + brk_offset);
	    ulabel *aul = (ulabel *) (text_va + brk_offset);
	    brk_offset += sizeof(*ul);
	    should_be(brk_offset <= dseg_len);

	    should_be(aul->ul_nent == ul->ul_nent);
	    should_be(aul->ul_default == ul->ul_default);
	    should_be(aul->ul_ent == (uint64_t *) (COOP_TEXT + brk_offset));
	    brk_offset += ul->ul_nent * sizeof(uint64_t);
	    should_be(brk_offset <= dseg_len);

	    should_be(!memcmp(aul->ul_ent, ul->ul_ent, ul->ul_nent * sizeof(uint64_t)));
	}
    }

    should_be(brk_offset == dseg_len);

    // === Arguments verified OK -- should be all good..
    *status_segp = status_seg;
}

static void
coop_gate_invoke_cleanup(label *tgt_label, label *tgt_clear, void *arg)
{
    delete tgt_label;
    delete tgt_clear;

    int *donep = (int *) arg;
    *donep = 1;
}

static void __attribute__((noreturn))
coop_gate_invoke_thread(int *invoke_donep, cobj_ref *gatep,
			label *cs, label *ds, label *dr,
			coop_sysarg arg_values[8])
{
    struct gate_call_data *gcd =
	(struct gate_call_data *) TLS_GATE_ARGS;
    struct coop_syscall_argval *csa_val =
	(struct coop_syscall_argval *) &gcd->param_buf[0];

    for (int i = 0; i < 8; i++)
	if (!arg_values[i].is_label)
	    csa_val->argval[i] = arg_values[i].u.i;

    label *tgt_label = new label();
    label *tgt_clear = new label();

    gate_compute_labels(*gatep, cs, ds, dr, tgt_label, tgt_clear);
    gate_invoke(*gatep, tgt_label, tgt_clear, &coop_gate_invoke_cleanup, invoke_donep);
}

int64_t
coop_gate_invoke(cobj_ref coop_gate,
		 label *cs, label *ds, label *dr,
		 coop_sysarg arg_values[8])
{
    cobj_ref status_seg;
    coop_verify(coop_gate, arg_values, &status_seg);

    label cur_label, cur_clear;
    thread_cur_label(&cur_label);
    thread_cur_clearance(&cur_clear);

    int64_t tid;
    error_check(tid = sys_thread_create(start_env->proc_container,
					"coop gate invoker"));

    cobj_ref tobj = COBJ(start_env->proc_container, tid);
    scope_guard<int, cobj_ref> thread_unref(sys_obj_unref, tobj);

    struct thread_entry te;
    memset(&te, 0, sizeof(te));

    int invoke_done = 0;
    error_check(sys_self_get_as(&te.te_as));
    te.te_entry = (void *) &coop_gate_invoke_thread;
    te.te_stack = tls_stack_top;
    te.te_arg[0] = (uint64_t) &invoke_done;
    te.te_arg[1] = (uint64_t) &coop_gate;
    te.te_arg[2] = (uint64_t) cs;
    te.te_arg[3] = (uint64_t) ds;
    te.te_arg[4] = (uint64_t) dr;
    te.te_arg[5] = (uint64_t) &arg_values[0];

    error_check(tid = sys_thread_start(tobj, &te,
				       cur_label.to_ulabel(),
				       cur_clear.to_ulabel()));

    struct coop_status *stat = 0;
    uint64_t stat_bytes = sizeof(*stat);
    error_check(segment_map(status_seg, 0, SEGMAP_READ,
			    (void **) &stat, &stat_bytes, 0));
    scope_guard<int, void *> unmap(segment_unmap, stat);

    while (!invoke_done || !stat->done)
	sys_sync_wait(&stat->done, 0, sys_clock_msec() + 1000);
    return stat->rval;
}
