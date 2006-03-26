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

void __attribute__((noreturn))
declassifier(void *arg, struct gate_call_data *gcd, gatesrv_return *gr)
{
    if (start_env->declassify_gate.object) {
	gate_call(start_env->declassify_gate, gcd, 0, 0, 0, 0);
	gr->ret(0, 0, 0);
    }

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
	darg->fs_create.name[sizeof(darg->fs_create.name) - 1] = '\0';
	darg->status = fs_create(darg->fs_create.dir,
				 &darg->fs_create.name[0],
				 &darg->fs_create.new_file);
    } else {
	cprintf("exit_declassifier: unknown request type %d\n", darg->req);
	darg->status = -E_BAD_OP;
    }

    gr->ret(0, 0, 0);
}
