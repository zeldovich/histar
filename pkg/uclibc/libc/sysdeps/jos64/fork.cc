extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/setjmp.h>
#include <inc/memlayout.h>
#include <inc/wait.h>

#include <unistd.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/scopeguard.hh>

static int fork_debug = 0;

static pid_t
do_fork()
{
    // Make all FDs independent of the process taint/grant handles
    for (int i = 0; i < MAXFD; i++)
	fd_make_public(i);

    // New process gets the same label as this process,
    // except we take away our process taint&grant * and
    // instead give it its own process taint&grant *.
    label thread_clearance, thread_contaminate;
    thread_cur_clearance(&thread_clearance);
    thread_cur_label(&thread_contaminate);

    if (fork_debug)
	cprintf("fork: current labels %s / %s\n",
		thread_contaminate.to_string(),
		thread_clearance.to_string());

    thread_contaminate.set(start_env->process_grant, 1);
    thread_contaminate.set(start_env->process_taint, 1);

    // Compute the mandatory contamination for objects
    label integrity_label(thread_contaminate);
    integrity_label.transform(label::star_to, integrity_label.get_default());

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

    // Create an exit status for it
    struct cobj_ref exit_status_seg;
    error_check(segment_alloc(top_ct, sizeof(struct process_state),
			      &exit_status_seg, 0,
			      integrity_label.to_ulabel(),
			      "exit status"));

    // Prepare a setjmp buffer for the new thread, before we copy our stack!
    struct jos_jmp_buf jb;
    if (jos_setjmp(&jb) != 0) {
	if (fork_debug)
	    cprintf("fork: new thread running\n");

	start_env->ppid = start_env->shared_container;

	start_env->shared_container = top_ct;
	start_env->proc_container = proc_ct;

	start_env->process_status_seg = exit_status_seg;

	start_env->process_grant = process_grant;
	start_env->process_taint = process_taint;

	pgrant_cleanup.dismiss();
	ptaint_cleanup.dismiss();
	top_drop.dismiss();

	signal_init();
	child_clear();

	return 0;
    }

    // Copy the address space, much like taint_cow() in lib/taint.c
    enum { uas_size = 64 };
    struct u_segment_mapping uas_ents[uas_size];
    struct u_address_space uas;
    uas.size = uas_size;
    uas.ents = &uas_ents[0];

    segment_unmap_flush();

    struct cobj_ref cur_as;
    error_check(sys_self_get_as(&cur_as));
    error_check(sys_as_get(cur_as, &uas));

    void *fd_base = (void *) UFDBASE;
    void *fd_end = ((char *) fd_base) + MAXFD * PGSIZE;

    for (uint32_t i = 0; i < uas.nent; i++) {
	if (uas.ents[i].flags == 0)
	    continue;
	if (uas.ents[i].segment.container == proc_ct)
	    continue;

	// FDs are a special case
	// XXX the fd refcounts are not garbage-collected on failure..
	void *va = uas.ents[i].va;
	if (va >= fd_base && va < fd_end) {
	    error_check(sys_segment_addref(uas.ents[i].segment, top_ct));
	    uas.ents[i].segment.container = top_ct;

	    struct Fd *fd = (struct Fd *) va;
	    if (!fd->fd_immutable)
		atomic_inc(&fd->fd_ref);
	}

	// What gets copied across fork() and what stays shared?
	// Our heuristic is that anything in the process container
	// gets copied (heap, stacks, data); everything else stays
	// shared.  This works well for FDs which are in the shared
	// container.
	if (uas.ents[i].segment.container != start_env->proc_container)
	    continue;

	char namebuf[KOBJ_NAME_LEN];
	error_check(sys_obj_get_name(uas.ents[i].segment, &namebuf[0]));

	int64_t id = sys_segment_copy(uas.ents[i].segment, proc_ct,
				      0, &namebuf[0]);
	error_check(id);

	uint64_t old_id = uas.ents[i].segment.object;
	for (uint32_t j = 0; j < uas.nent; j++)
	    if (uas.ents[j].segment.object == old_id)
		uas.ents[j].segment = COBJ(proc_ct, id);
    }

    // Construct the new AS object and a non-running thread
    int64_t id = sys_as_create(proc_ct, 0, "forked AS");
    error_check(id);

    if (fork_debug) {
	cprintf("fork: new AS:\n");
	segment_map_print(&uas);
    }

    struct cobj_ref new_as = COBJ(proc_ct, id);
    error_check(sys_as_set(new_as, &uas));

    id = sys_thread_create(proc_ct, "forked thread");
    error_check(id);
    struct cobj_ref new_th = COBJ(proc_ct, id);

    // Create a new thread that jumps into the new AS
    struct thread_entry te;
    te.te_as = new_as;
    te.te_entry = (void *) &jos_longjmp;
    te.te_stack = 0;
    te.te_arg = (uint64_t) &jb;

    if (fork_debug)
	cprintf("fork: new thread labels %s / %s\n",
		thread_contaminate.to_string(),
		thread_clearance.to_string());

    error_check(sys_thread_start(new_th, &te,
				 thread_contaminate.to_ulabel(),
				 thread_clearance.to_ulabel()));

    top_drop.dismiss();

    child_add(top_ct, exit_status_seg);
    return top_ct;
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
