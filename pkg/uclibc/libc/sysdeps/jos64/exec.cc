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
    serial_vec(char *p, uint32_t n) : p_(p), n_(n), c_(0)  {}

    void add(const char *s) {
	size_t len = strlen(s) + 1;
	if (len > n_)
            throw error(-E_NO_SPACE, "serial_vec out of space");
        n_ -= len;
    	memcpy(p_, s, len);
    	p_ += len;
	c_++;
    }

    void add_all(char *const *v) {
	for (uint32_t i = 0; v[i]; i++)
	    add(v[i]);
    }

    char *p_;
    uint32_t n_;
    uint32_t c_;

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
    strncpy(cmd, segbuf + 2, s - 1);
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
					  secret_label.to_ulabel(), &buf[0],
					  0, CT_QUOTA_INF);
    error_check(proc_ct);

    cobj_ref proc_ref = COBJ(start_env->shared_container, proc_ct);
    scope_guard<int, cobj_ref> proc_drop(sys_obj_unref, proc_ref);

    // Create an environment
    uint64_t env_size = PGSIZE;
    start_env_t *new_env = 0;
    struct cobj_ref new_env_ref;
    error_check(segment_alloc(proc_ct, env_size, &new_env_ref,
			      (void**) &new_env, 0, "env"));
    scope_guard<int, void *> new_env_unmap(segment_unmap, new_env);

    memcpy(new_env, start_env, sizeof(*new_env));
    new_env->proc_container = proc_ct;

    int room = env_size - sizeof(start_env_t);
    char *p = &new_env->args[0];
    serial_vec sv(p, room);
    
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

    sv.add_all(argv);
    new_env->argc = sv.c_;

    sv.c_ = 0;
    sv.add_all(envp);
    new_env->envc = sv.c_;

    // Map environment in new address space
    void *new_env_va = 0;
    error_check(segment_map_as(e.te_as, new_env_ref,
			       0, SEGMAP_READ | SEGMAP_WRITE,
			       &new_env_va, 0, 0));
    e.te_arg[0] = (uint64_t) new_env_va;

    // Create a thread
    int64_t tid = sys_thread_create(proc_ct, &name[0]);
    error_check(tid);
    struct cobj_ref th_ref = COBJ(proc_ct, tid);
    
    // want to carry trace over
    new_env->trace_on = debug_gate_trace();
    
    // Start it!
    error_check(sys_thread_start(th_ref, &e,
				 thread_contaminate.to_ulabel(),
				 thread_clearance.to_ulabel()));

    // Die
    proc_drop.dismiss();
    close_all();
    signal_gate_close();
    debug_gate_close();
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
        do_execve(bin, filename, argv, envp);
    } catch (std::exception &e) {
        cprintf("execve: %s\n", e.what());
        __set_errno(EINVAL);
        return -1;
    }   
}
