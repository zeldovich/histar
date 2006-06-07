extern "C" {
#include <inc/syscall.h>
#include <inc/cooperate.h>
#include <inc/memlayout.h>
#include <inc/gateparam.h>
#include <inc/assert.h>

#include <string.h>
}

#include <inc/cooperate.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

#include <inc/gateclnt.hh>

extern char cooperate_syscall[], cooperate_syscall_end[];

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

		ul_copy->ul_ent = (uint64_t *) (text_va + coop_brk_offset);
		coop_brk_offset += ul->ul_nent * sizeof(uint64_t);

		ul_copy->ul_nent = ul->ul_nent;
		ul_copy->ul_default = ul->ul_default;
		memcpy(ul_copy->ul_ent, ul->ul_ent, ul->ul_nent * sizeof(uint64_t));

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

    struct u_segment_mapping usm[3] = {
	{ status_seg, 0, 1, 0,
	  SEGMAP_READ | SEGMAP_WRITE, (void *) COOP_STATUS },
	{ text_seg,   0, ROUNDUP(data_seg_len, PGSIZE) / PGSIZE, 0,
	  SEGMAP_READ | SEGMAP_EXEC,  (void *) COOP_TEXT },
	{ COBJ(0, kobject_id_thread_sg), 0, 1, 0,
	  SEGMAP_READ | SEGMAP_WRITE, (void *) UTLS },
	};
    struct u_address_space uas = { 3, 3, &usm[0] };
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

int64_t
coop_gate_invoke(cobj_ref coop_gate,
		 label *cs, label *ds, label *dr,
		 coop_sysarg arg_values[8])
{
    struct gate_call_data gcd;
    struct coop_syscall_argval *csa_val =
	(struct coop_syscall_argval *) &gcd.param_buf[0];

    for (int i = 0; i < 8; i++) {
	if (arg_values[i].is_label)
	    throw basic_exception("Unbound label arguments not supported.");
	csa_val->argval[i] = arg_values[i].u.i;
    }

    gate_call(coop_gate, cs, ds, dr).call(&gcd, 0);
    return 0;

/*
    struct gate_call_data *gcd_tls = (struct gate_call_data *) tls_gate_args;
    struct coop_syscall_argval *csa_val =
	(struct coop_syscall_argval *) &gcd_tls->param_buf[0];
*/

    throw basic_exception("hmm");
}
