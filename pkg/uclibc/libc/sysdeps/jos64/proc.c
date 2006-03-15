#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <inc/syscall.h>
#include <string.h>

#include <bits/unimpl.h>

pid_t
getpid()
{
    return start_env->shared_container;
}

pid_t
getppid()
{
    return start_env->ppid;
}

int
nice(int n)
{
    return 0;
}

int
setpgid(pid_t pid, pid_t pgid)
{
    set_enosys();
    return -1;
}

pid_t
__getpgid(pid_t pid)
{
    set_enosys();
    return -1;
}

pid_t
getpgrp(void)
{
    return __getpgid(0);
}

pid_t
wait4(pid_t pid, int *statusp, int options, struct rusage *rusage)
{
    if (pid <= 0) {
	set_enosys();
	return -1;
    }

    uint64_t ct = pid;
    int64_t id = container_find(ct, kobj_segment, "exit status");
    if (id < 0) {
	__set_errno(ESRCH);
	return -1;
    }

    uint64_t proc_status = PROCESS_RUNNING;
    uint64_t exit_code;

    while (proc_status == PROCESS_RUNNING) {
	struct cobj_ref psobj = COBJ(ct, id);
	struct process_state *ps = 0;
	int r = segment_map(psobj, SEGMAP_READ, (void **) &ps, 0);
	if (r < 0) {
	    __set_errno(EPERM);
	    return -1;
	}

	if (ps->status == PROCESS_RUNNING && (options & WNOHANG)) {
	    segment_unmap(ps);
	    __set_errno(ECHILD);
	    return -1;
	}

	sys_sync_wait(&ps->status, PROCESS_RUNNING, sys_clock_msec() + 10000);
	proc_status = ps->status;
	exit_code = ps->exit_code;
	segment_unmap(ps);
    }

    union wait wstat;
    memset(&wstat, 0, sizeof(wstat));
    wstat.w_termsig = 0;
    wstat.w_retcode = exit_code;

    if (statusp)
	*statusp = wstat.w_status;
    return 0;
}
