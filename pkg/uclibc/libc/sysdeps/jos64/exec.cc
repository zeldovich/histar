extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/memlayout.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <bits/unimpl.h>
#include <bits/signalgate.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

static void __attribute__((noreturn))
do_execve(fs_inode bin, char *const *argv)
{
    // Reuse the top-level container and process taint/grant labels,
    // but create a new "process" container in the top-level container.

    label thread_contaminate, thread_clearance;
    thread_cur_label(&thread_contaminate);
    thread_cur_clearance(&thread_clearance);

    label secret_label(thread_contaminate);
    secret_label.transform(label::star_to, secret_label.get_default());
    secret_label.set(start_env->process_grant, 0);
    secret_label.set(start_env->process_taint, 3);

    // Figure out the name
    char name[KOBJ_NAME_LEN];
    error_check(sys_obj_get_name(bin.obj, &name[0]));

    char buf[KOBJ_NAME_LEN];
    snprintf(&buf[0], KOBJ_NAME_LEN, "exec:%s", &name[0]);

    // Allocate new process container
    int64_t proc_ct = sys_container_alloc(start_env->shared_container,
					  secret_label.to_ulabel(), &buf[0]);
    error_check(proc_ct);

    cobj_ref proc_ref = COBJ(start_env->shared_container, proc_ct);
    scope_guard<int, cobj_ref> proc_drop(sys_obj_unref, proc_ref);

    // Load ELF binary into container
    thread_entry e;
    memset(&e, 0, sizeof(e));
    error_check(elf_load(proc_ct, bin.obj, &e, secret_label.to_ulabel()));

    // Move our file descriptors over to the new process
    for (uint32_t i = 0; i < MAXFD; i++) {
	struct Fd *fd;
	uint64_t fd_flags;

	int r = fd_lookup(i, &fd, 0, &fd_flags);
	if (r < 0)
	    continue;
	if ((fd_flags & SEGMAP_CLOEXEC))
	    continue;

	error_check(dup2_as(i, i, e.te_as, start_env->shared_container));
    }

    // Create an initial properly-labeled heap
    struct cobj_ref heap_obj;
    error_check(segment_alloc(proc_ct, 0, &heap_obj, 0,
			      secret_label.to_ulabel(), "heap"));

    // Create an environment
    start_env_t *new_env = 0;
    struct cobj_ref new_env_ref;
    error_check(segment_alloc(proc_ct, PGSIZE, &new_env_ref,
			      (void**) &new_env,
			      secret_label.to_ulabel(), "env"));
    scope_guard<int, void *> new_env_unmap(segment_unmap, new_env);

    memcpy(new_env, start_env, sizeof(*new_env));
    new_env->proc_container = proc_ct;

    char *p = &new_env->args[0];
    for (int i = 0; argv[i]; i++) {
	size_t len = strlen(argv[i]);
	memcpy(p, argv[i], len);
	p += len + 1;
    }

    // Map environment in new address space
    void *new_env_va = 0;
    error_check(segment_map_as(e.te_as, new_env_ref,
			       SEGMAP_READ | SEGMAP_WRITE,
			       &new_env_va, 0));
    e.te_arg[0] = (uint64_t) new_env_va;

    // Create a thread
    int64_t tid = sys_thread_create(proc_ct, &name[0]);
    error_check(tid);
    struct cobj_ref th_ref = COBJ(proc_ct, tid);

    // Start it!
    error_check(sys_thread_start(th_ref, &e,
				 thread_contaminate.to_ulabel(),
				 thread_clearance.to_ulabel()));

    // Die
    proc_drop.dismiss();
    close_all();
    signal_gate_close();
    sys_obj_unref(COBJ(start_env->shared_container, start_env->proc_container));
    thread_halt();
}

int
execve(const char *filename, char *const *argv, char *const *envp) __THROW
{
    try {
	fs_inode bin;
	int r = fs_namei(filename, &bin);
	if (r < 0) {
	    __set_errno(ENOENT);
	    return -1;
	}

	do_execve(bin, argv);
    } catch (std::exception &e) {
	cprintf("execve: %s\n", e.what());
	__set_errno(EINVAL);
	return -1;
    }
}
