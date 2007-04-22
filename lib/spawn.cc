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
#include <unistd.h>
#include <signal.h>
}

#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>
#include <inc/gateclnt.hh>

static int label_debug = 0;

struct child_process
spawn(spawn_descriptor *sd)
{
    label tmp, out;
    bool autogrant = !(sd->spawn_flags_ & SPAWN_NO_AUTOGRANT);
    bool uinit_style = (sd->spawn_flags_ & SPAWN_UINIT_STYLE);

    // Compute receive label for new process
    label thread_clear(2);

    thread_cur_clearance(&tmp);
    thread_clear.merge(&tmp, &out, label::min, label::leq_starhi);
    thread_clear = out;

    if (sd->cr_) {
	thread_clear.merge(sd->cr_, &out, label::min, label::leq_starhi);
	thread_clear = out;
    }
    if (sd->dr_) {
	thread_clear.merge(sd->dr_, &out, label::max, label::leq_starhi);
	thread_clear = out;
    }

    // Compute send label for new process
    label thread_label(1);

    thread_cur_label(&tmp);
    tmp.transform(label::star_to, 0);
    thread_label.merge(&tmp, &out, label::max, label::leq_starlo);
    thread_label = out;

    if (sd->cs_) {
	thread_label.merge(sd->cs_, &out, label::max, label::leq_starlo);
	thread_label = out;
    }
    if (sd->ds_) {
	thread_label.merge(sd->ds_, &out, label::min, label::leq_starlo);
	thread_label = out;
    }

    // Objects for new process are effectively the same label, except
    // we can drop the stars altogether -- they're discretionary.
    label integrity_object_label(thread_label);
    integrity_object_label.transform(label::star_to,
				     integrity_object_label.get_default());
    label proc_object_label(integrity_object_label);
    if (sd->co_) {
	proc_object_label.merge(sd->co_, &out, label::max, label::leq_starlo);
	proc_object_label = out;
    }

    // Generate some private handles for the new process
    int64_t process_grant = handle_alloc();
    error_check(process_grant);
    scope_guard<void, uint64_t> pgrant_cleanup(thread_drop_star, process_grant);

    int64_t process_taint = handle_alloc();
    error_check(process_taint);
    scope_guard2<void, uint64_t, uint64_t> ptaint_cleanup(thread_drop_starpair, process_taint, process_grant);
    pgrant_cleanup.dismiss();

    if (!uinit_style) {
	thread_label.set(process_grant, LB_LEVEL_STAR);
	thread_label.set(process_taint, LB_LEVEL_STAR);
	thread_clear.set(process_grant, 3);
	thread_clear.set(process_taint, 3);
    }

    if (autogrant && start_env->user_grant && start_env->user_taint) {
	thread_label.set(start_env->user_grant, LB_LEVEL_STAR);
	thread_label.set(start_env->user_taint, LB_LEVEL_STAR);
	thread_clear.set(start_env->user_grant, 3);
	thread_clear.set(start_env->user_taint, 3);
    }

    if (!uinit_style) {
	proc_object_label.set(process_grant, 0);
	proc_object_label.set(process_taint, 3);
	integrity_object_label.set(process_grant, 0);
    } else {
	proc_object_label.set(start_env->user_grant, 0);
	integrity_object_label.set(start_env->user_grant, 0);
    }

    // Now spawn with computed labels
    struct cobj_ref elf;
    fs_get_obj(sd->elf_ino_, &elf);

    char name[KOBJ_NAME_LEN];
    error_check(sys_obj_get_name(elf, &name[0]));

    int64_t c_top = sys_container_alloc(sd->ct_,
					integrity_object_label.to_ulabel(),
					&name[0], 0, CT_QUOTA_INF);
    error_check(c_top);

    struct cobj_ref c_top_ref = COBJ(sd->ct_, c_top);
    scope_guard<int, struct cobj_ref> c_top_drop(sys_obj_unref, c_top_ref);

    int64_t c_proc = sys_container_alloc(c_top,
					 proc_object_label.to_ulabel(),
					 "process", 0, CT_QUOTA_INF);
    error_check(c_proc);

    struct thread_entry e;
    memset(&e, 0, sizeof(e));
    error_check(elf_load(c_proc, elf, &e, 0));

    int fdnum[3] = { sd->fd0_, sd->fd1_, sd->fd2_ };
    for (int i = 0; !uinit_style && i < 3; i++) {
	struct Fd *fd;
	error_check(fd_lookup(fdnum[i], &fd, 0, 0));
	error_check(dup2_as(fdnum[i], i, e.te_as, c_top));

	thread_label.set(fd->fd_handle[fd_handle_taint], LB_LEVEL_STAR);
	thread_clear.set(fd->fd_handle[fd_handle_taint], 3);
	if (fd->fd_handle[fd_handle_extra_taint]) {
	    thread_label.set(fd->fd_handle[fd_handle_extra_taint], LB_LEVEL_STAR);
	    thread_clear.set(fd->fd_handle[fd_handle_extra_taint], 3);
	}
	if (!fd->fd_immutable) {
	    thread_label.set(fd->fd_handle[fd_handle_grant], LB_LEVEL_STAR);
	    thread_clear.set(fd->fd_handle[fd_handle_grant], 3);
	}
	if (!fd->fd_immutable && fd->fd_handle[fd_handle_extra_grant]) {
	    thread_label.set(fd->fd_handle[fd_handle_extra_grant], LB_LEVEL_STAR);
	    thread_clear.set(fd->fd_handle[fd_handle_extra_grant], 3);
	}
    }

    uint64_t env_size = PGSIZE;
    start_env_t *spawn_env = 0;
    struct cobj_ref spawn_env_obj;
    error_check(segment_alloc(c_proc, env_size, &spawn_env_obj,
			      (void**) &spawn_env, 0, "env"));
    scope_guard<int, void *> spawn_env_unmap(segment_unmap, spawn_env);

    void *spawn_env_va = 0;
    error_check(segment_map_as(e.te_as, spawn_env_obj,
			       0, SEGMAP_READ | SEGMAP_WRITE,
			       &spawn_env_va, 0, 0));

    struct cobj_ref exit_status_seg;
    error_check(segment_alloc(c_top, sizeof(struct process_state),
			      &exit_status_seg, 0, 0,
			      "exit status"));

    memcpy(spawn_env, start_env, sizeof(*spawn_env));
    spawn_env->proc_container = c_proc;
    spawn_env->shared_container = c_top;
    spawn_env->process_grant = process_grant;
    spawn_env->process_taint = process_taint;
    spawn_env->process_status_seg = exit_status_seg;
    spawn_env->ppid = 0;
    if (sd->fs_mtab_seg_.object)
	spawn_env->fs_mtab_seg = sd->fs_mtab_seg_;
    if (sd->fs_root_.obj.object)
	spawn_env->fs_root = sd->fs_root_;
    if (sd->fs_cwd_.obj.object)
	spawn_env->fs_cwd = sd->fs_cwd_;

    if (!uinit_style) {
	uint64_t *child_pgid = 0;
	label pgid_label(1); 
	pgid_label.set(start_env->user_grant, 0);
	try {
	    error_check(segment_alloc(c_top, sizeof(uint64_t),
				      &spawn_env->process_gid_seg, (void **) &child_pgid, 
				      pgid_label.to_ulabel(), "process gid"));
	} catch (error &e) {
	    if (e.err() != -E_LABEL)
		throw e;
	    thread_cur_label(&tmp);
	    pgid_label.set(start_env->user_grant, 1);
	    pgid_label.set(process_grant, 0);
	    pgid_label.merge(&tmp, &out, label::max, label::leq_starlo);
	    pgid_label = out;
	    error_check(segment_alloc(c_top, sizeof(uint64_t),
				      &spawn_env->process_gid_seg, (void **) &child_pgid, 
				      pgid_label.to_ulabel(), "process gid"));
	}
	scope_guard<int, void *> pgid_unmap(segment_unmap, child_pgid);
	*child_pgid = getpgrp();
    }

    int room = env_size - sizeof(start_env_t);
    char *p = &spawn_env->args[0];
    for (int i = 0; i < sd->ac_; i++) {
    	size_t len = strlen(sd->av_[i]) + 1;
    	room -= len;
        if (room < 0)
            throw error(-E_NO_SPACE, "args overflow env");
        memcpy(p, sd->av_[i], len);
    	p += len;
    }
    spawn_env->argc = sd->ac_;

    for (int i = 0; i < sd->envc_; i++) {
        size_t len = strlen(sd->envv_[i]) + 1; 
        room -= len;
        if (room < 0)
            throw error(-E_NO_SPACE, "env vars overflow env");
        memcpy(p, sd->envv_[i], len);
        p += len;
    }
    spawn_env->envc = sd->envc_;

    uint64_t thread_ct = uinit_style ? c_top : c_proc;
    int64_t thread = sys_thread_create(thread_ct, &name[0]);
    error_check(thread);
    struct cobj_ref tobj = COBJ(thread_ct, thread);

    if (label_debug) {
	thread_cur_label(&tmp);
	printf("spawn: current label %s\n", tmp.to_string());

	thread_cur_clearance(&tmp);
	printf("spawn: current clearance: %s\n", tmp.to_string());

	printf("spawn: starting thread with label %s, clear %s\n",
	       thread_label.to_string(), thread_clear.to_string());
    }

    if (uinit_style) {
	e.te_arg[0] = 1;
	e.te_arg[1] = sd->ct_;
	e.te_arg[2] = start_env->user_grant;
    } else {
	e.te_arg[0] = 0;
	e.te_arg[1] = (uint64_t) spawn_env_va;
    }
    error_check(sys_thread_start(tobj, &e,
				 thread_label.to_ulabel(),
				 thread_clear.to_ulabel()));

    struct child_process child;
    child.container = c_top;
    child.wait_seg = exit_status_seg;

    c_top_drop.dismiss();
    return child;
}

struct child_process
spawn(uint64_t container, struct fs_inode elf_ino,
      int fd0, int fd1, int fd2,
      int ac, const char **av,
      int envc, const char **envv,
      label *cs, label *ds, label *cr, label *dr,
      label *contaminate_object,
      int spawn_flags, 
      struct cobj_ref fs_mtab_seg)
{
    spawn_descriptor sd;
    sd.ct_ = container;
    sd.elf_ino_ = elf_ino;
    sd.fd0_ = fd0;
    sd.fd1_ = fd1;
    sd.fd2_ = fd2;
    sd.ac_ = ac;
    sd.av_ = av;
    sd.envc_ = envc;
    sd.envv_ = envv;
    sd.cs_ = cs;
    sd.ds_ = ds;
    sd.cr_ = cr;
    sd.dr_ = dr;
    sd.co_ = contaminate_object;
    sd.spawn_flags_ = spawn_flags;
    sd.fs_mtab_seg_ = fs_mtab_seg;
    return spawn(&sd);
}

int
process_wait(const struct child_process *child, int64_t *exit_code)
{
    uint64_t proc_status = PROCESS_RUNNING;

    while (proc_status == PROCESS_RUNNING) {
	struct process_state *ps = 0;
	int r = segment_map(child->wait_seg, 0, SEGMAP_READ, (void **) &ps, 0, 0);
	if (r < 0)
	    return r;

	sys_sync_wait(&ps->status, PROCESS_RUNNING, sys_clock_nsec() + NSEC_PER_SECOND * 10);
	proc_status = ps->status;
	if (proc_status == PROCESS_EXITED && exit_code)
	    *exit_code = ps->exit_code;
	segment_unmap(ps);
    }

    return proc_status;
}

static int
process_update_state(uint64_t state, int64_t exit_code, int64_t signo)
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
		    0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **) &ps, 0, 0);
    if (r < 0)
	return r;

    ps->exit_code = exit_code;
    ps->exit_signal = signo;
    ps->status = state;
    sys_sync_wakeup(&ps->status);
    segment_unmap(ps);
    return 0;
}

int
process_report_taint(void)
{
    return process_update_state(PROCESS_TAINTED, 0, 0);
}

int
process_report_exit(int64_t code, int64_t signo)
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

	    gate_call(start_env->declassify_gate, 0, &ds, 0).call(&gcd, &ds);
	} catch (std::exception &e) {
	    cprintf("process_report_exit: %s\n", e.what());
	    return -1;
	}

	return 0;
    }

    int r = process_update_state(PROCESS_EXITED, code, signo);
    if (r >= 0 && start_env->ppid)
	kill(start_env->ppid, SIGCHLD);
    return r;
}
