extern "C" {
#include <inc/lib.h>
#include <inc/exit_declass.h>
#include <inc/gateparam.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

#include <signal.h>

void
exit_declassifier(void *arg, struct gate_call_data *gcd, gatesrv_return *gr)
{
    struct exit_declass_args *darg =
	(struct exit_declass_args *) &gcd->param_buf[0];

    struct process_state *ps = 0;
    int r = segment_map(darg->status_seg, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &ps, 0);
    if (r >= 0) {
	ps->status = PROCESS_TAINTED_EXIT;
	segment_unmap(ps);
    }

    kill(darg->parent_pid, SIGCHLD);
    gr->ret(0, 0, 0);
}
