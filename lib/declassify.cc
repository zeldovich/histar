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
declassifier(uint64_t arg, struct gate_call_data *gcd, gatesrv_return *gr)
{
#if 0
    uint64_t declassify_cat = arg;

    label vo, vc;
    thread_cur_verify(&vo, &vc);
    if (declass_debug)
	cprintf("declassify: verify owner %s clear %s\n", vo.to_string(), vc.to_string());

    /*
     * This doesn't seem to be the right way to do this anyway..
     * What happens if we're tainted with two categories, and two
     * declassifiers are needed to reveal exit status?
     */
    if (start_env->declassify_gate.object) {
	gate_call(start_env->declassify_gate, 0, 0).call(gcd, &declassified);
	gr->new_ret(0, 0);
    }
#endif

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
	int r = segment_map(darg->exit.status_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &ps, 0, 0);
	if (r >= 0) {
	    ps->status = PROCESS_TAINTED_EXIT;
	    segment_unmap(ps);
	}

	kill(darg->exit.parent_pid, SIGCHLD);
    } else if (darg->req == declassify_fs_create) {
	label file_label;
	thread_cur_label(&file_label);

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

    gr->new_ret(0, 0);
}
