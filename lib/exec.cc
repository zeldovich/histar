extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/debug_gate.h>

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

struct serial_vec {
 public:
    serial_vec(char *p, uint64_t cursize, uint64_t maxsize, cobj_ref seg)
	: p_(p), cursize_(cursize), maxsize_(maxsize), seg_(seg), c_(0)
    {
	used_ = sizeof(start_env_t);
    }

    void add(const char *s) {
	uint64_t len = strlen(s) + 1;
	if (used_ + len > maxsize_)
            throw error(-E_NO_SPACE, "serial_vec out of space");
	if (used_ + len > cursize_) {
	    uint64_t newsize = MIN(cursize_ * 2, maxsize_);
	    error_check(sys_segment_resize(seg_, newsize));
	    cursize_ = newsize;
	}
        used_ += len;
    	memcpy(p_, s, len);
    	p_ += len;
	c_++;
    }

    void add_all(char *const *v) {
	for (uint32_t i = 0; v[i]; i++)
	    add(v[i]);
    }

    char *p_;
    uint64_t cursize_;
    uint64_t maxsize_;
    uint64_t used_;
    cobj_ref seg_;
    uint64_t c_;

 private:
    serial_vec(const serial_vec&);
    serial_vec &operator=(const serial_vec&);
};

static int
script_load(uint64_t container, struct cobj_ref seg, struct thread_entry *e,
	    serial_vec *avec)
{
    uint64_t seglen = 0;
    char *segbuf = 0;
    
    int r = segment_map(seg, 0, SEGMAP_READ, (void**)&segbuf, &seglen, 0);
    if (r < 0) {
	cprintf("script_load: unable to map segment\n");
	return r;
    }
    scope_guard<int, void *> unmap(segment_unmap, segbuf);

    if (segbuf[0] != '#' || segbuf[1] !='!')
	return -E_INVAL;
    
    // Read #! command
    char *end = strchr(segbuf, '\n');
    uint32_t s = end - segbuf - 1;
    char cmd[64];
    if (s > sizeof(cmd)) {
	cprintf("script_load: command too long\n");
	return -E_INVAL;
    }

    char *segptr = segbuf + 2;
    while (*segptr == ' ' || *segptr == '\t') {
	segptr++;
	s--;
    }

    strncpy(cmd, segptr, s - 1);
    cmd[s - 1] = 0;
    
    char *arg = cmd;
    for (uint32_t i = 0; i <= s - 1; i++) {
	if (cmd[i] == ' ') {
	    cmd[i] = 0;
	    if (strlen(arg))
		avec->add(arg);
	    arg = (&cmd[i]) + 1;
	} else if (!cmd[i]) {
	    if (strlen(arg))
		avec->add(arg);
	}
    }
        
    // Load ELF for #! command
    fs_inode bin;
    r = fs_namei(cmd, &bin);
    if (r < 0) {
	cprintf("script_load: unknown command %s\n", cmd);
	return r;
    }
    
    return elf_load(container, bin.obj, e, 0);
}

static void __attribute__((noreturn))
do_execve(fs_inode bin, const char *fn, char *const *argv, char *const *envp)
{
    // Make all file descriptors have their own taint and grant handles
    for (int i = 0; i < MAXFD; i++)
	fd_make_public(i, 0);

    // Reuse the top-level container and process taint/grant labels,
    // but create a new "process" container in the top-level container.

    label thread_label, thread_owner, thread_clear;
    thread_cur_label(&thread_label);
    thread_cur_ownership(&thread_owner);
    thread_cur_clearance(&thread_clear);

    label secret_label(thread_label);
    secret_label.add(start_env->process_grant);
    secret_label.add(start_env->process_taint);

    // Figure out the name
    char name[KOBJ_NAME_LEN];
    error_check(sys_obj_get_name(bin.obj, &name[0]));

    char buf[KOBJ_NAME_LEN];
    snprintf(&buf[0], KOBJ_NAME_LEN, "exec:%s", &name[0]);

    // Allocate new process container
    int64_t proc_ct = sys_container_alloc(start_env->shared_container,
					  secret_label.to_ulabel(), &buf[0],
					  0, CT_QUOTA_INF);
    error_check(proc_ct);

    cobj_ref proc_ref = COBJ(start_env->shared_container, proc_ct);
    scope_guard<int, cobj_ref> proc_drop(sys_obj_unref, proc_ref);

    // Create an environment
    uint64_t env_size = PGSIZE;
    start_env_t *new_env = 0;
    int64_t segid = sys_segment_create(proc_ct, env_size, 0, "env");
    error_check(segid);

    uint64_t map_size = 256 * PGSIZE;
    struct cobj_ref new_env_ref = COBJ(proc_ct, segid);
    error_check(segment_map(new_env_ref, 0, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &new_env, &map_size, 0));
    scope_guard<int, void *> new_env_unmap(segment_unmap, new_env);

    memcpy(new_env, start_env, sizeof(*new_env));
    new_env->proc_container = proc_ct;

    char *p = &new_env->args[0];
    serial_vec sv(p, env_size, map_size, new_env_ref);
    
    // Load ELF binary into container
    thread_entry e;
    memset(&e, 0, sizeof(e));

    int r = elf_load(proc_ct, bin.obj, &e, 0);
    if (r < 0) {
	error_check(script_load(proc_ct, bin.obj, &e, &sv));

	// replace argv[0] with guaranteed full pathname
	sv.add(fn);
	if (argv[0])
	    argv++;
    }

    // Place args, environment
    sv.add_all(argv);
    new_env->argc = sv.c_;

    sv.c_ = 0;
    sv.add_all(envp);
    new_env->envc = sv.c_;

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

    // Map environment in new address space
    void *new_env_va = 0;
    error_check(segment_map_as(e.te_as, new_env_ref,
			       0, SEGMAP_READ | SEGMAP_WRITE,
			       &new_env_va, 0, 0));
    e.te_arg[0] = 0;
    e.te_arg[1] = (uintptr_t) new_env_va;

    // Create a thread
    int64_t tid = sys_thread_create(proc_ct, &name[0], thread_label.to_ulabel());
    error_check(tid);
    struct cobj_ref th_ref = COBJ(proc_ct, tid);
    
    // want to carry trace over
    new_env->trace_on = debug_gate_trace();

    // Change the title of the process
    struct process_state *procstat = 0;
    error_check(segment_map(start_env->process_status_seg,
			    0, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &procstat, 0, 0));
    strncpy(&procstat->procname[0], &name[0], sizeof(procstat->procname));
    segment_unmap_delayed(procstat, 1);
    
    // Start it!
    error_check(sys_thread_start(th_ref, &e,
				 thread_owner.to_ulabel(),
				 thread_clear.to_ulabel()));

    // Die
    proc_drop.dismiss();
    close_all();
    signal_gate_close();
    debug_gate_close();
    sys_obj_unref(COBJ(start_env->shared_container, start_env->proc_container));
    thread_halt();
}

libc_hidden_proto(execve)

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
        do_execve(bin, filename, argv, envp);
    } catch (std::exception &e) {
        cprintf("execve: %s\n", e.what());
        __set_errno(EINVAL);
        return -1;
    }   
}

libc_hidden_def(execve)

