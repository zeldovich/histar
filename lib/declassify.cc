extern "C" {
#include <inc/lib.h>
#include <inc/declassify.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/error.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

#include <signal.h>

enum { declass_debug = 0 };

void __attribute__((noreturn))
declassifier(void *arg, struct gate_call_data *gcd, gatesrv_return *gr)
{
    uint64_t declassify_handle = (uint64_t) arg;

    label verify;
    thread_cur_verify(&verify);
    if (declass_debug)
	cprintf("declassify: verify label %s\n", verify.to_string());

    label declassified(verify);
    declassified.set(declassify_handle, declassified.get_default());

    if (start_env->declassify_gate.object) {
	gate_call(start_env->declassify_gate, 0, &declassified, 0).call(gcd, &declassified);
	gr->ret(0, 0, 0);
    }

    // XXX
    // would be nice if we could change our label to something like
    // verify + return-handle, and perform whatever declassification
    // actions in that context?

    struct declassify_args *darg =
	(struct declassify_args *) &gcd->param_buf[0];
    static_assert(sizeof(*darg) <= sizeof(gcd->param_buf));
    darg->status = 0;

    // XXX
    // somehow need to avoid confused deputy problem here -- should only
    // exercise the caller's privilege when updating their exit status
    // segment and delivering SIGCHLD.

    if (darg->req == declassify_exit) {
	struct process_state *ps = 0;
	int r = segment_map(darg->exit.status_seg, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &ps, 0);
	if (r >= 0) {
	    ps->status = PROCESS_TAINTED_EXIT;
	    segment_unmap(ps);
	}

	kill(darg->exit.parent_pid, SIGCHLD);
    } else if (darg->req == declassify_fs_create) {
	label file_label(verify);
	file_label.transform(label::star_to, file_label.get_default());

	if (declass_debug)
	    cprintf("declassify: file label %s\n", file_label.to_string());

	darg->fs_create.name[sizeof(darg->fs_create.name) - 1] = '\0';
	darg->status = fs_create(darg->fs_create.dir,
				 &darg->fs_create.name[0],
				 &darg->fs_create.new_file,
				 file_label.to_ulabel());
    } else if (darg->req == declassify_fs_resize) {
	darg->status = fs_resize(darg->fs_resize.ino, darg->fs_resize.len);
    } else {
	cprintf("exit_declassifier: unknown request type %d\n", darg->req);
	darg->status = -E_BAD_OP;
    }

    gr->ret(0, 0, 0);
}
