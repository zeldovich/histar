#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/wait.h>
#include <inc/queue.h>
#include <inc/debug_gate.h>

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
    uint64_t wc_sig_gen;
    LIST_ENTRY(wait_child) wc_link;
};

static LIST_HEAD(, wait_child) live_children;
static LIST_HEAD(, wait_child) free_children;
static uint64_t child_counter;

enum { child_debug = 0 };

// Add a child.  Used by parent fork process.
void
child_add(pid_t pid, struct cobj_ref status_seg)
{
    struct wait_child *wc = LIST_FIRST(&free_children);
    if (wc) {
	LIST_REMOVE(wc, wc_link);
    } else {
	wc = malloc(sizeof(*wc));
	if (wc == 0) {
	    cprintf("child_add: out of memory\n");
	    return;
	}
    }

    wc->wc_pid = pid;
    wc->wc_seg = status_seg;
    wc->wc_sig_gen = 0;
    LIST_INSERT_HEAD(&live_children, wc, wc_link);
}

// Remove all children.  Used by child fork process.
void
child_clear()
{
    struct wait_child *wc, *next;
    for (wc = LIST_FIRST(&live_children); wc; wc = next) {
	next = LIST_NEXT(wc, wc_link);
	LIST_REMOVE(wc, wc_link);
	free(wc);
    }

    for (wc = LIST_FIRST(&free_children); wc; wc = next) {
	next = LIST_NEXT(wc, wc_link);
	LIST_REMOVE(wc, wc_link);
	free(wc);
    }
}

void
child_notify()
{
    child_counter++;
    sys_sync_wakeup(&child_counter);
}

// Returns 1 if child has exited, 0 if still running
static int
child_get_status(struct wait_child *wc, int *statusp)
{
    struct process_state *ps = 0;
    int r = segment_map(wc->wc_seg, 0, SEGMAP_READ, (void **) &ps, 0, 0);
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

static int
child_get_siginfo(struct wait_child *wc, int *statusp)
{
    uint64_t ct = wc->wc_pid;
    int64_t seg_id = container_find(ct, kobj_segment, "debug info");
    if (seg_id < 0)
	return 0;
    
    struct debug_info *dinfo = 0;
    int r = segment_map(COBJ(ct, seg_id), 0, SEGMAP_READ, 
			(void **) &dinfo, 0, 0);
    if (r < 0) {
	cprintf("child_get_siginfo: unable to map debug info: %s\n", e2s(r));
	return 0;
    }

    char signo = dinfo->signo;
    uint64_t gen = dinfo->gen;
    
    int ret = 0;
    if (signo && gen != wc->wc_sig_gen) {
	union wait wstat;
	wc->wc_sig_gen = gen;
	memset(&wstat, 0, sizeof(wstat));
	wstat.w_status = W_STOPCODE(signo);
	if (statusp)
	    *statusp = wstat.w_status;
	ret = 1;
    }
    segment_unmap_delayed(dinfo, 1);
    return ret;
}

// Actual system call emulated on jos64
pid_t
wait4(pid_t pid, int *statusp, int options, struct rusage *rusage)
{
    struct wait_child *wc, *next;
    uint64_t start_counter;

again:
    start_counter = child_counter;
    if (child_debug)
	cprintf("[%ld] wait4: counter %ld\n", thread_id(), start_counter);

    for (wc = LIST_FIRST(&live_children); wc; wc = next) {
	next = LIST_NEXT(wc, wc_link);

	if (pid >= 0 && pid != wc->wc_pid)
	    continue;

	int r = child_get_status(wc, statusp);
	if (child_debug)
	    cprintf("[%ld] wait4: child %ld status %d\n",
		    thread_id(), wc->wc_pid, r);
	if (r < 0) {
	    // Bad child?
	    LIST_REMOVE(wc, wc_link);
	    LIST_INSERT_HEAD(&free_children, wc, wc_link);
	    continue;
	}

	if (r == 0) {
	    r = child_get_siginfo(wc, statusp);
	    if (child_debug)
		cprintf("[%ld] wait4: child %ld siginfo %d\n",
			thread_id(), wc->wc_pid, r);
	    if (r == 1)
		return wc->wc_pid;
	    continue;
	}

	if (r == 1) {
	    // Child exited
	    pid = wc->wc_pid;
	    LIST_REMOVE(wc, wc_link);
	    LIST_INSERT_HEAD(&free_children, wc, wc_link);

	    // Clean up the child process's container
	    sys_obj_unref(COBJ(start_env->shared_container, pid));

	    if (child_debug)
		cprintf("[%ld] wait4: returning child %ld\n",
			thread_id(), pid);
	    return pid;
	}
    }

    if (!(options & WNOHANG)) {
	if (child_debug)
	    cprintf("[%ld] wait4: waiting..\n", thread_id());

	sys_sync_wait(&child_counter, start_counter, sys_clock_msec() + 1000);
	goto again;
    }

    if (child_debug)
	cprintf("[%ld] wait4: returning ECHILD\n", thread_id());

    __set_errno(ECHILD);
    return -1;
}
