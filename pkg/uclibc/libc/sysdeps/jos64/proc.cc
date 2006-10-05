extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <bits/unimpl.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

static const char proc_debug = 1;

pid_t
getpid() __THROW
{
    return start_env->shared_container;
}

pid_t
getppid() __THROW
{
    return start_env->ppid;
}

int
nice(int n) __THROW
{
    return 0;
}

#if 0
int
setpgid(pid_t pid, pid_t pgid) __THROW
{
    
    cprintf("setpgid: pid %ld pgid %ld getpid() %ld\n", pid, pgid, getpid());
    __set_errno(ENOSYS);
    return -1;
}
#endif

int
setpgid(pid_t pid, pid_t new_pgid) __THROW
{
    if (pid == 0)
	pid = getpid();
    if (new_pgid == 0)
	new_pgid = getpid();

    int64_t seg_id = container_find(pid, kobj_segment, "process gid");
    if (seg_id < 0) {
	if (proc_debug)
	    cprintf("setpgid: cannot find process gid segment in %ld: %s\n",
		    pid, e2s(seg_id));
	__set_errno(ESRCH);
	return -1;
    }

    try {
	uint64_t *pgid = 0;
	error_check(segment_map(COBJ(pid, seg_id), 0, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &pgid, 0, 0));
	scope_guard<int, void*> seg_unmap(segment_unmap, pgid);

	*pgid = new_pgid;

	return 0;
    } catch (std::exception &e) {
	if (proc_debug)
	    cprintf("setpgid: cannot map segment: %s", e.what());
	__set_errno(EPERM);
    }
    return -1;
}

pid_t
__getpgid(pid_t pid) __THROW
{
    if (pid == 0)
	pid = getpid();

    int64_t seg_id = container_find(pid, kobj_segment, "process gid");
    if (seg_id == -E_NOT_FOUND) 
	return pid;
    else if (seg_id < 0) {
	if (proc_debug)
	    cprintf("__getpgid: cannot find process gid segment in %ld: %s\n",
		    pid, e2s(seg_id));
	__set_errno(ESRCH);
	return -1;
    }
    
    try {
	uint64_t *pgid = 0;
	error_check(segment_map(COBJ(pid, seg_id), 0, SEGMAP_READ,
				(void **) &pgid, 0, 0));
	scope_guard<int, void*> seg_unmap(segment_unmap, pgid);
	return *pgid;
    } catch (std::exception &e) {
	if (proc_debug)
	    cprintf("__getpgid: cannot map segment: %s", e.what());
	__set_errno(EPERM);
    }
    return -1;
}

pid_t
getpgrp(void) __THROW
{
    return __getpgid(0);
}

pid_t
setsid(void) __THROW
{
    setpgid(0, 0);
    return 0;
}
