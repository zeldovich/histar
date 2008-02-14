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
#include <inttypes.h>

libc_hidden_proto(wait4)

struct wait_child {
    pid_t wc_pid;
    struct cobj_ref wc_seg;
    uint64_t wc_sig_gen;
    uint64_t wc_handled_stops; /* last count of child stops as seen by parent */
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
    wc->wc_handled_stops = 0;
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
    int64_t pid_mask = UINT64(~0);

again:
    start_counter = child_counter;
    if (child_debug)
	cprintf("[%"PRIu64"] wait4: counter %"PRIu64"\n", thread_id(), start_counter);

    int found_pid = 0;
    for (wc = LIST_FIRST(&live_children); wc; wc = next) {
        struct process_state *ps = 0;
        int r;
        uint64_t status, stops;
        int64_t exit_code, exit_signal;
	next = LIST_NEXT(wc, wc_link);

	if (pid >= 0 && pid != (wc->wc_pid & pid_mask))
	    continue;

	found_pid = 1;
        r = segment_map(wc->wc_seg, 0, SEGMAP_READ,
                        (void **) &ps, 0, 0);
        if (r < 0) {
            __set_errno(ESRCH);
	    // Bad child?
	    LIST_REMOVE(wc, wc_link);
	    LIST_INSERT_HEAD(&free_children, wc, wc_link);
	    continue;
        }

	if (child_debug)
	    cprintf("[%"PRIu64"] wait4: child %"PRIu64" map ok? %d\n",
		    thread_id(), wc->wc_pid, r);

        status = ps->status;
        stops = ps->stops;
        exit_code = ps->exit_code;
        exit_signal = ps->exit_signal;
        segment_unmap(ps);

	if (child_debug)
	    cprintf("[%"PRIu64"] wait4: child %"PRIu64" status %"PRId64"\n",
		    thread_id(), wc->wc_pid, status);

        /* Report stopped only once to parent by catching up handled_stops */
        if (options & WUNTRACED &&
            status == PROCESS_STOPPED &&
            stops != wc->wc_handled_stops) {
            if (child_debug)
                cprintf("[%"PRIu64"] wait4: child %"PRIu64" handling stop "
                        "ps stops %"PRIu64" handled stops %"PRIu64"\n",
                        thread_id(), wc->wc_pid, stops, wc->wc_handled_stops);
            union wait wstat;
            wc->wc_sig_gen = exit_signal;
            memset(&wstat, 0, sizeof(wstat));
            wstat.w_status = W_STOPCODE(exit_signal);
            if (statusp)
                *statusp = wstat.w_status;
            wc->wc_handled_stops = stops;
            return wc->wc_pid;
        }

        if (status == PROCESS_RUNNING) {
            if (child_debug)
                cprintf("[%"PRIu64"] wait4: child %"PRIu64" RUNNING\n",
                        thread_id(), wc->wc_pid);
	    r = child_get_siginfo(wc, statusp);
	    if (child_debug)
		cprintf("[%"PRIu64"] wait4: child %"PRIu64" siginfo %d\n",
			thread_id(), wc->wc_pid, r);
	    if (r == 1)
		return wc->wc_pid;
	    continue;
	}

        if (status == PROCESS_EXITED) {
            if (child_debug)
                cprintf("[%"PRIu64"] wait4: child %"PRIu64" EXITED\n",
                        thread_id(), wc->wc_pid);
            union wait wstat;
            memset(&wstat, 0, sizeof(wstat));
            wstat.w_termsig = exit_signal;
            wstat.w_coredump = 0;
            wstat.w_retcode = exit_code;
            if (statusp)
                *statusp = wstat.w_status;
	    // Child exited
	    pid = wc->wc_pid;
	    LIST_REMOVE(wc, wc_link);
	    LIST_INSERT_HEAD(&free_children, wc, wc_link);

	    // Clean up the child process's container
	    int64_t pidparent = sys_container_get_parent(pid);
	    if (pidparent >= 0)
		sys_obj_unref(COBJ(pidparent, pid));

	    if (child_debug)
		cprintf("[%"PRIu64"] wait4: returning child %"PRIu64"\n",
			thread_id(), pid);
	    return pid;
        }
        if (child_debug)
            cprintf("[%"PRIu64"] wait4: child %"PRIu64" status transition "
                    "not handled\n",
                    thread_id(), wc->wc_pid);
    }

    if (pid >= 0 && !found_pid) {
	cprintf("[%"PRIu64"/%"PRIu64"] wait4: %s: dud pid %"PRIu64"\n",
		thread_id(), getpid(), jos_progname, pid);
        if (pid_mask != 0xFFFFFFFF) {
            pid_mask = 0xFFFFFFFF;
            cprintf("[%"PRIu64"/%"PRIu64"] wait4: %s: retrying with 32-bit "
                    "pid_mask %"PRIu64"\n",
                    thread_id(), getpid(), jos_progname, pid);
            goto again;
        }
	__set_errno(ECHILD);
	return -1;
    }

    if (!(options & WNOHANG)) {
	if (child_debug)
	    cprintf("[%"PRIu64"] wait4: waiting..\n", thread_id());

	sys_sync_wait(&child_counter, start_counter,
		      sys_clock_nsec() + NSEC_PER_SECOND);
	goto again;
    }

    if (child_debug)
	cprintf("[%"PRIu64"] wait4: returning ECHILD\n", thread_id());

    __set_errno(ECHILD);
    return -1;
}

libc_hidden_def(wait4)

