#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/fd.h>
#include <inc/utrap.h>
#include <inc/gateparam.h>
#include <inc/debug_gate.h>
#include <inc/prof.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern int main(int argc, const char **argv, char **envp);

uint64_t start_arg0;
uint64_t start_arg1;
start_env_t *start_env;

void *tls_top;
uint64_t *tls_tidp;
struct jos_jmp_buf **tls_pgfault;
struct jos_jmp_buf **tls_pgfault_all;
void *tls_gate_args;
void *tls_stack_top;
void *tls_base;

int setup_env_done;

#define MAXARGS	128

static int argc;
static const char **argv;

void __attribute__((noinline))
setup_env(uint64_t envaddr, uint64_t arg1)
{
    if (arg1)
	return;

    // This process has enough of an environment,
    // unlike a bootstrap process.
    start_env = (start_env_t *) envaddr;
    start_env->taint_cow_as = COBJ(0, 0);
    prof_init(0);

    extern const char *__progname;
    __progname = &start_env->args[0];

    struct u_segment_mapping usm;
    struct cobj_ref start_env_seg;
    int r = segment_lookup(start_env, &usm);
    if (r < 0 || r == 0)
	panic("libmain: cannot find start_env segment: %s", e2s(r));

    start_env_seg = usm.segment;
    void *start_env_ro = (void *) USTARTENVRO;
    r = segment_map(start_env_seg, 0, SEGMAP_READ, &start_env_ro, 0, 0);
    if (r < 0)
	panic("libmain: cannot map start_env_ro: %s", e2s(r));

    struct cobj_ref tls = COBJ(0, kobject_id_thread_sg);
    tls_base = (void *) UTLSBASE;
    tls_top = (void *) UTLSTOP;
    uint64_t tls_mapbytes = tls_top - tls_base;
    r = segment_map(tls, 0, SEGMAP_READ | SEGMAP_WRITE | SEGMAP_REVERSE_PAGES,
		    &tls_base, &tls_mapbytes, 0);
    if (r < 0)
	panic("libmain: cannot map tls: %s", e2s(r));

    tls_tidp = tls_top - sizeof(uint64_t);
    tls_pgfault = tls_top - sizeof(uint64_t) - sizeof(*tls_pgfault);
    tls_pgfault_all = tls_top - sizeof(uint64_t) - sizeof(*tls_pgfault) - sizeof(*tls_pgfault_all);
    tls_gate_args = tls_top - sizeof(uint64_t) - sizeof(*tls_pgfault) - sizeof(*tls_pgfault_all) - sizeof(struct gate_call_data);
    assert(tls_gate_args == (void *) TLS_GATE_ARGS);

    // AMD64 ABI requires 16-byte stack frame alignment
    tls_stack_top = ROUNDDOWN(tls_gate_args, 16);

    assert(0 == sys_container_move_quota(start_env->proc_container,
					 thread_id(), thread_quota_slush));
    assert(0 == sys_obj_set_fixedquota(COBJ(0, thread_id())));

    r = utrap_init();
    if (r < 0)
	panic("libmain: cannot setup utrap: %s", e2s(r));

    signal_init();
    debug_gate_init();

    argv = malloc(sizeof(*argv) * (start_env->argc + 1));
    if (!argv)
	panic("libmain: out of memory for argv array");
    argv[start_env->argc] = 0;

    const char *p = &start_env->args[0];
    for (int i = 0; i < start_env->argc; i++) {
    	size_t len = strlen(p);
	argv[argc] = p;
	argc++;
	p += len + 1;
    }

    // make sure to initialize environ
    setenv(" ", " ", 0);
    for (int i = 0; i < start_env->envc; i++) {
        size_t len = strlen(p);
        char *value = strpbrk(p, "=");
        *value = 0;
        value++;
        setenv(p, value, 1);
        p += len + 1;
    }

    setup_env_done = 1;
}

void
libmain(uint64_t arg0, uint64_t arg1)
{
    start_arg0 = arg0;
    start_arg1 = arg1;

    if (start_arg1 == 0) {
	if (start_env->trace_on) {
	    debug_gate_trace_is(1);
	    debug_gate_breakpoint();
	}
    }

    exit(main(argc, &argv[0], environ));
}

void
tls_revalidate(void)
{
    if (tls_tidp)
	*tls_tidp = sys_self_id();
    if (tls_pgfault)
	*tls_pgfault = 0;
    if (tls_pgfault_all)
	*tls_pgfault_all = 0;
}
