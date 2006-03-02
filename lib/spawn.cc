extern "C" {
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/fd.h>
}

#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

static int label_debug = 0;

uint64_t
spawn(uint64_t container, struct fs_inode elf_ino,
      int fd0, int fd1, int fd2,
      int ac, const char **av,
      label *cs, label *ds, label *cr, label *dr,
      uint64_t flags)
{
    label tmp;

    // Compute receive label for new process
    label thread_clear(2);

    thread_cur_clearance(&tmp);
    thread_clear.merge_with(&tmp, label::min, label::leq_starhi);

    if (cr)
	thread_clear.merge_with(cr, label::min, label::leq_starhi);
    if (dr)
	thread_clear.merge_with(dr, label::max, label::leq_starhi);

    // Compute send label for new process
    label thread_label(1);

    thread_cur_label(&tmp);
    tmp.transform(label::star_to, 0);
    thread_label.merge_with(&tmp, label::max, label::leq_starlo);

    if (cs)
	thread_label.merge_with(cs, label::max, label::leq_starlo);
    if (ds)
	thread_label.merge_with(ds, label::min, label::leq_starlo);

    // Objects for new process are effectively the same label, except
    // we can drop the stars altogether -- they're discretionary.
    label object_label(thread_label);
    object_label.transform(label::star_to, object_label.get_default());

    // Generate a private handle for the new process
    int64_t process_handle = sys_handle_create();
    if (process_handle < 0)
	throw error(process_handle, "alloc handle");
    scope_guard<void, uint64_t> handle_cleanup(thread_drop_star, process_handle);

    object_label.set(process_handle, 0);
    thread_label.set(process_handle, LB_LEVEL_STAR);

    // Now spawn with computed labels
    struct cobj_ref elf;
    error_check(fs_get_obj(elf_ino, &elf));

    char name[KOBJ_NAME_LEN];
    error_check(sys_obj_get_name(elf, &name[0]));

    int64_t c_spawn = sys_container_alloc(container,
					  object_label.to_ulabel(), &name[0]);
    if (c_spawn < 0)
	throw error(c_spawn, "allocate container");

    struct cobj_ref c_spawn_ref = COBJ(container, c_spawn);
    scope_guard<int, struct cobj_ref> c_spawn_drop(sys_obj_unref, c_spawn_ref);

    struct thread_entry e;
    error_check(elf_load(c_spawn, elf, &e, object_label.to_ulabel()));

    int fdnum[3] = { fd0, fd1, fd2 };

    if ((flags & SPAWN_MOVE_FD)) {
	int i, j;

	label fd_label(object_label);
	fd_label.set(process_handle, fd_label.get_default());

	// Find all of the source FD's, increment refcounts
	struct Fd *fd[3];
	struct cobj_ref src_seg[3];
	for (i = 0; i < 3; i++) {
	    error_check(fd_lookup(fdnum[i], &fd[i], &src_seg[i]));
	    atomic_inc(&fd[i]->fd_ref);
	}

	// Drop refcounts on the fd, one per real FD object, and unmap
	for (i = 0; i < 3; i++) {
	    for (j = 0; j < i; j++)
		if (src_seg[i].object == src_seg[j].object)
		    break;

	    if (i == j)
		atomic_dec(&fd[i]->fd_ref);
	    fd_unmap(fd[i]);
	}

	// Move the FDs into the new container, and map them
	struct cobj_ref dst_seg[3];
	for (i = 0; i < 3; i++) {
	    for (j = 0; j < i; j++)
		if (src_seg[i].object == src_seg[j].object)
		    break;

	    if (i == j) {
		if (label_debug)
		    printf("spawn: copying fd with label %s\n",
			   fd_label.to_string());

		int64_t id = sys_segment_copy(src_seg[i], c_spawn,
					      fd_label.to_ulabel(),
					      "moved fd");
		if (id < 0)
		    throw error(id, "fd segment copy");

		sys_obj_unref(src_seg[i]);
		dst_seg[i] = COBJ(c_spawn, id);
	    }

	    error_check(fd_map_as(e.te_as, dst_seg[j], i));
	}
    } else {
	for (int i = 0; i < 3; i++)
	    error_check(dup_as(fdnum[i], i, e.te_as));
    }

    struct cobj_ref heap_obj;
    error_check(segment_alloc(c_spawn, 0, &heap_obj, 0,
			      object_label.to_ulabel(), "heap"));

    start_env_t *spawn_env = 0;
    struct cobj_ref c_spawn_env;
    error_check(segment_alloc(c_spawn, PGSIZE, &c_spawn_env,
			      (void**) &spawn_env, object_label.to_ulabel(),
			      "env"));
    scope_guard<int, void *> spawn_env_unmap(segment_unmap, spawn_env);

    void *spawn_env_va = 0;
    error_check(segment_map_as(e.te_as, c_spawn_env,
			       SEGMAP_READ | SEGMAP_WRITE,
			       &spawn_env_va, 0));

    struct cobj_ref exit_status_seg;
    error_check(segment_alloc(c_spawn, PGSIZE, &exit_status_seg,
			      0, object_label.to_ulabel(), "exit status"));

    memcpy(spawn_env, start_env, sizeof(*spawn_env));
    spawn_env->container = c_spawn;

    char *p = &spawn_env->args[0];
    for (int i = 0; i < ac; i++) {
	size_t len = strlen(av[i]);
	memcpy(p, av[i], len);
	p += len + 1;
    }

    int64_t thread = sys_thread_create(c_spawn, &name[0]);
    if (thread < 0)
	throw error(thread, "creating thread");
    struct cobj_ref tobj = COBJ(c_spawn, thread);

    if (label_debug)
	printf("spawn: starting thread with label %s\n",
	       thread_label.to_string());

    e.te_arg = (uint64_t) spawn_env_va;
    error_check(sys_thread_start(tobj, &e,
				 thread_label.to_ulabel(),
				 thread_clear.to_ulabel()));

    c_spawn_drop.dismiss();
    return c_spawn;
}

int
process_wait(uint64_t childct, int64_t *exit_code)
{
    uint64_t proc_status = PROCESS_RUNNING;

    while (proc_status == PROCESS_RUNNING) {
	int64_t obj = container_find(childct, kobj_segment, "exit status");
	if (obj < 0)
	    return obj;

	struct process_state *ps = 0;
	int r = segment_map(COBJ(childct, obj), SEGMAP_READ, (void **) &ps, 0);
	if (r < 0)
	    return r;

	sys_thread_sync_wait(&ps->status, PROCESS_RUNNING, sys_clock_msec() + 10000);
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
    int64_t obj = container_find(start_env->container, kobj_segment, "exit status");
    if (obj < 0)
	return obj;

    struct process_state *ps = 0;
    int r = segment_map(COBJ(start_env->container, obj),
			SEGMAP_READ | SEGMAP_WRITE,
			(void **) &ps, 0);
    if (r < 0)
	return r;

    ps->exit_code = exit_code;
    ps->status = state;
    sys_thread_sync_wakeup(&ps->status);
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
    return process_update_state(PROCESS_EXITED, code);
}
