#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/wait.h>
#include <inc/queue.h>

#include <sys/resource.h>
#include <sys/wait.h>
#include <bits/unimpl.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

struct wait_child {
    pid_t wc_pid;
    struct cobj_ref wc_seg;
    LIST_ENTRY(wait_child) wc_link;
};

static LIST_HEAD(wc_head, wait_child) children;

// Add a child.  Used by parent fork process.
void
child_add(pid_t pid, struct cobj_ref status_seg)
{
    struct wait_child *wc = malloc(sizeof(*wc));
    if (wc == 0) {
	cprintf("child_add: out of memory\n");
	return;
    }

    wc->wc_pid = pid;
    wc->wc_seg = status_seg;
    LIST_INSERT_HEAD(&children, wc, wc_link);
}

// Remove all children.  Used by child fork process.
void
child_clear()
{
    struct wait_child *wc, *next;
    for (wc = LIST_FIRST(&children); wc; wc = next) {
	next = LIST_NEXT(wc, wc_link);
	LIST_REMOVE(wc, wc_link);
	free(wc);
    }
}

// Returns 1 if child has exited, 0 if still running
static int
child_get_status(struct wait_child *wc, int *statusp)
{
    struct process_state *ps = 0;
    int r = segment_map(wc->wc_seg, SEGMAP_READ, (void **) &ps, 0);
    if (r < 0) {
	__set_errno(ESRCH);
	return -1;
    }

    uint64_t status = ps->status;
    int64_t exit_code = ps->exit_code;
    segment_unmap(ps);

    if (status == PROCESS_RUNNING)
	return 0;

    union wait wstat;
    memset(&wstat, 0, sizeof(wstat));
    wstat.w_termsig = 0;
    wstat.w_retcode = exit_code;

    if (statusp)
	*statusp = wstat.w_status;

    return 1;
}

// Actual system call emulated on jos64
pid_t
wait4(pid_t pid, int *statusp, int options, struct rusage *rusage)
{
    struct wait_child *wc, *next;
again:
    for (wc = LIST_FIRST(&children); wc; wc = next) {
	next = LIST_NEXT(wc, wc_link);

	if (pid >= 0 && pid != wc->wc_pid)
	    continue;

	int r = child_get_status(wc, statusp);
	if (r < 0) {
	    // Bad child?
	    LIST_REMOVE(wc, wc_link);
	    free(wc);
	    continue;
	}

	if (r == 0)
	    continue;

	if (r == 1) {
	    // Child exited
	    LIST_REMOVE(wc, wc_link);
	    pid = wc->wc_pid;
	    free(wc);

	    // Clean up the child process's container
	    sys_obj_unref(COBJ(start_env->shared_container, pid));

	    return pid;
	}
    }

    if (!(options & WNOHANG)) {
	thread_sleep(1000);
	goto again;
    }

    __set_errno(ECHILD);
    return -1;
}
