#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/prof.h>
#include <inc/syscall.h>
#include <inc/signal.h>

#include <signal.h>
#include <unistd.h>

void
process_exit(int64_t rval, int64_t signo)
{
    close_all();

    prof_print(1);

    if (start_env) {
	process_report_exit(rval, signo);

	if (start_env->ppid) {
	    /*
	     * Wait until our parent dies, and GC this zombie process.
	     * Until then, the parent has a chance to wait() on us.
	     */
	    utrap_set_mask(1);
	    signal_shutdown();

	    struct cobj_ref ppid_ct = COBJ(start_env->ppid, start_env->ppid);
	    while (sys_obj_get_type(ppid_ct) == kobj_container)
		sleep(10);

	    uint64_t parent_ct =
		sys_container_get_parent(start_env->shared_container);
	    sys_obj_unref(COBJ(parent_ct, start_env->shared_container));
	}
    }

    thread_halt();
}

void
_exit(int rval)
{
    process_exit(rval, 0);
}
