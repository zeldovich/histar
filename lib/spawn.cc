extern "C" {
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/gateparam.h>
#include <inc/declassify.h>

#include <string.h>
}

#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>
#include <inc/gateclnt.hh>

#include <signal.h>

static int label_debug = 0;

struct child_process
spawn(uint64_t container, struct fs_inode elf_ino,
      int fd0, int fd1, int fd2,
      int ac, const char **av,
      int envc, const char **envv,
      label *cs, label *ds, label *cr, label *dr)
{
    label tmp, out;

    // Compute receive label for new process
    label thread_clear(2);

    thread_cur_clearance(&tmp);
    thread_clear.merge(&tmp, &out, label::min, label::leq_starhi);
    thread_clear.copy_from(&out);

    if (cr) {
	thread_clear.merge(cr, &out, label::min, label::leq_starhi);
	thread_clear.copy_from(&out);
    }
    if (dr) {
	thread_clear.merge(dr, &out, label::max, label::leq_starhi);
	thread_clear.copy_from(&out);
    }

    // Compute send label for new process
    label thread_label(1);

    thread_cur_label(&tmp);
    tmp.transform(label::star_to, 0);
    thread_label.merge(&tmp, &out, label::max, label::leq_starlo);
    thread_label.copy_from(&out);

    if (cs) {
	thread_label.merge(cs, &out, label::max, label::leq_starlo);
	thread_label.copy_from(&out);
    }
    if (ds) {
	thread_label.merge(ds, &out, label::min, label::leq_starlo);
	thread_label.copy_from(&out);
    }

    // Objects for new process are effectively the same label, except
    // we can drop the stars altogether -- they're discretionary.
    label integrity_object_label(thread_label);
    integrity_object_label.transform(label::star_to,
				     integrity_object_label.get_default());
    label proc_object_label(integrity_object_label);

    // Generate some private handles for the new process
    int64_t process_grant = sys_handle_create();
    error_check(process_grant);
    scope_guard<void, uint64_t> pgrant_cleanup(thread_drop_star, process_grant);

    int64_t process_taint = sys_handle_create();
    error_check(process_taint);
    scope_guard<void, uint64_t> ptaint_cleanup(thread_drop_star, process_taint);

    thread_label.set(process_grant, LB_LEVEL_STAR);
    thread_label.set(process_taint, LB_LEVEL_STAR);
    if (start_env->user_grant && start_env->user_taint) {
	thread_label.set(start_env->user_grant, LB_LEVEL_STAR);
	thread_label.set(start_env->user_taint, LB_LEVEL_STAR);
    }
    proc_object_label.set(process_grant, 0);
    proc_object_label.set(process_taint, 3);
    integrity_object_label.set(process_grant, 0);

    // Now spawn with computed labels
    struct cobj_ref elf;
    fs_get_obj(elf_ino, &elf);

    char name[KOBJ_NAME_LEN];
    error_check(sys_obj_get_name(elf, &name[0]));

    int64_t c_top = sys_container_alloc(container,
					integrity_object_label.to_ulabel(),
					&name[0]);
    error_check(c_top);

    struct cobj_ref c_top_ref = COBJ(container, c_top);
    scope_guard<int, struct cobj_ref> c_top_drop(sys_obj_unref, c_top_ref);

    int64_t c_proc = sys_container_alloc(c_top,
					 proc_object_label.to_ulabel(),
					 "process");
    error_check(c_proc);

    struct thread_entry e;
    memset(&e, 0, sizeof(e));
    error_check(elf_load(c_proc, elf, &e, proc_object_label.to_ulabel()));

    int fdnum[3] = { fd0, fd1, fd2 };
    for (int i = 0; i < 3; i++) {
	struct Fd *fd;
	error_check(fd_lookup(fdnum[i], &fd, 0, 0));
	error_check(dup2_as(fdnum[i], i, e.te_as, c_top));
	thread_label.set(fd->fd_taint, LB_LEVEL_STAR);
	if (!fd->fd_immutable)
	    thread_label.set(fd->fd_grant, LB_LEVEL_STAR);
    }

    struct cobj_ref heap_obj;
    error_check(segment_alloc(c_proc, 0, &heap_obj, 0,
			      proc_object_label.to_ulabel(), "heap"));

    start_env_t *spawn_env = 0;
    struct cobj_ref spawn_env_obj;
    error_check(segment_alloc(c_proc, PGSIZE, &spawn_env_obj,
			      (void**) &spawn_env,
			      proc_object_label.to_ulabel(),
			      "env"));
    scope_guard<int, void *> spawn_env_unmap(segment_unmap, spawn_env);

    void *spawn_env_va = 0;
    error_check(segment_map_as(e.te_as, spawn_env_obj,
			       SEGMAP_READ | SEGMAP_WRITE,
			       &spawn_env_va, 0));

    struct cobj_ref exit_status_seg;
    error_check(segment_alloc(c_top, sizeof(struct process_state),
			      &exit_status_seg, 0,
			      integrity_object_label.to_ulabel(),
			      "exit status"));

    memcpy(spawn_env, start_env, sizeof(*spawn_env));
    spawn_env->proc_container = c_proc;
    spawn_env->shared_container = c_top;
    spawn_env->process_grant = process_grant;
    spawn_env->process_taint = process_taint;
    spawn_env->process_status_seg = exit_status_seg;

    char *p = &spawn_env->args[0];
    for (int i = 0; i < ac; i++) {
    	size_t len = strlen(av[i]);
    	memcpy(p, av[i], len);
    	p += len + 1;
    }
    
    p++;
    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envv[i]);    
        memcpy(p, envv[i], len);
        p += len + 1;
    }
    
    

    int64_t thread = sys_thread_create(c_proc, &name[0]);
    error_check(thread);
    struct cobj_ref tobj = COBJ(c_proc, thread);

    if (label_debug) {
	thread_cur_label(&tmp);
	printf("spawn: current label %s\n", tmp.to_string());

	thread_cur_clearance(&tmp);
	printf("spawn: current clearance: %s\n", tmp.to_string());

	printf("spawn: starting thread with label %s, clear %s\n",
	       thread_label.to_string(), thread_clear.to_string());
    }

    e.te_arg[0] = (uint64_t) spawn_env_va;
    error_check(sys_thread_start(tobj, &e,
				 thread_label.to_ulabel(),
				 thread_clear.to_ulabel()));

    struct child_process child;
    child.container = c_top;
    child.wait_seg = exit_status_seg;

    c_top_drop.dismiss();
    return child;
}

int
process_wait(struct child_process *child, int64_t *exit_code)
{
    uint64_t proc_status = PROCESS_RUNNING;

    while (proc_status == PROCESS_RUNNING) {
	struct process_state *ps = 0;
	int r = segment_map(child->wait_seg, SEGMAP_READ, (void **) &ps, 0);
	if (r < 0)
	    return r;

	sys_sync_wait(&ps->status, PROCESS_RUNNING, sys_clock_msec() + 10000);
	proc_status = ps->status;
	if (proc_status == PROCESS_EXITED && exit_code)
	    *exit_code = ps->exit_code;
	segment_unmap(ps);
    }

    return proc_status;
}

static int
process_update_state(uint64_t state, int64_t exit_code)
{
    label lseg, lcur;
    try {
	thread_cur_label(&lcur);
	obj_get_label(start_env->process_status_seg, &lseg);
    } catch (error &e) {
	cprintf("process_update_state: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("process_update_state: %s\n", e.what());
	return -E_INVAL;
    }

    int r = lcur.compare(&lseg, label::leq_starlo);
    if (r < 0)
	return r;

    struct process_state *ps = 0;
    r = segment_map(start_env->process_status_seg,
		    SEGMAP_READ | SEGMAP_WRITE,
		    (void **) &ps, 0);
    if (r < 0)
	return r;

    ps->exit_code = exit_code;
    ps->status = state;
    sys_sync_wakeup(&ps->status);
    segment_unmap(ps);
    return 0;
}

int
process_report_taint(void)
{
    return process_update_state(PROCESS_TAINTED, 0);
}

int
process_report_exit(int64_t code)
{
    if (start_env->declassify_gate.object) {
	try {
	    struct gate_call_data gcd;
	    struct declassify_args *darg =
		(struct declassify_args *) &gcd.param_buf[0];
	    darg->req = declassify_exit;
	    darg->exit.status_seg = start_env->process_status_seg;
	    darg->exit.parent_pid = start_env->ppid;

	    // Grant the exit declassifier our process grant handle,
	    // so that it can write to our process status segment.
	    label ds(3);
	    ds.set(start_env->process_grant, LB_LEVEL_STAR);

	    gate_call(start_env->declassify_gate, &gcd, 0, &ds, 0, &ds);
	} catch (std::exception &e) {
	    cprintf("process_report_exit: %s\n", e.what());
	    return -1;
	}

	return 0;
    }

    int r = process_update_state(PROCESS_EXITED, code);
    if (r >= 0)
	kill(start_env->ppid, SIGCHLD);
    return r;
}
