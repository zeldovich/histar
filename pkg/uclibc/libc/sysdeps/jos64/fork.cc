extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <unistd.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/scopeguard.hh>

static pid_t
do_fork()
{
    // New process gets the same label as this process,
    // except we take away our process taint&grant * and
    // instead give it its own process taint&grant *.
    label thread_clearance, thread_contaminate;
    thread_cur_clearance(&thread_clearance);
    thread_cur_label(&thread_contaminate);

    thread_clearance.set(start_env->process_grant, 1);
    thread_clearance.set(start_env->process_taint, 1);

    // Compute the mandatory contamination for objects
    label integrity_label(thread_contaminate);
    integrity_label.transform(label::star_to, 1);

    label secret_label(integrity_label);

    // Generate handles for new process
    int64_t process_grant = sys_handle_create();
    error_check(process_grant);
    scope_guard<void, uint64_t> pgrant_cleanup(thread_drop_star, process_grant);

    int64_t process_taint = sys_handle_create();
    error_check(process_taint);
    scope_guard<void, uint64_t> ptaint_cleanup(thread_drop_star, process_taint);

    // Grant process handles to the new process
    thread_contaminate.set(process_grant, LB_LEVEL_STAR);
    thread_contaminate.set(process_taint, LB_LEVEL_STAR);
    secret_label.set(process_grant, 0);
    secret_label.set(process_taint, 3);
    integrity_label.set(process_grant, 0);

    // Start creating the new process
    int64_t top_ct = sys_container_alloc(start_env->shared_container,
					 integrity_label.to_ulabel(),
					 "forked");
    error_check(top_ct);

    struct cobj_ref top_ref = COBJ(start_env->shared_container, top_ct);
    scope_guard<int, struct cobj_ref> top_drop(sys_obj_unref, top_ref);

    int64_t proc_ct = sys_container_alloc(top_ct, secret_label.to_ulabel(),
					  "process");
    error_check(proc_ct);

    // ...

    throw basic_exception("do_fork: not implemented");
}

pid_t
fork(void) __THROW
{
    try {
	return do_fork();
    } catch (std::exception &e) {
	cprintf("fork: %s\n", e.what());
	__set_errno(ENOMEM);
	return -1;
    }
}
