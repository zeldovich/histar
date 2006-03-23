#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/fd.h>
#include <inc/utrap.h>
#include <inc/gateparam.h>
#include <string.h>
#include <stdlib.h>

extern int main(int argc, const char **argv);

uint64_t start_arg0;
uint64_t start_arg1;
start_env_t *start_env;
struct fs_mount_table *fs_mtab;

uint64_t *tls_tidp;
void *tls_gate_args;
void *tls_stack_top;
void *tls_base;

#define MAXARGS	16

static int argc;
static const char *argv[MAXARGS];

static void
__attribute__((noinline))
setup_env(uint64_t envaddr)
{
    // This process has enough of an environment,
    // unlike a bootstrap process.
    start_env = (start_env_t *) envaddr;

    const char *p = &start_env->args[0];
    for (int i = 0; i < MAXARGS; i++) {
	size_t len = strlen(p);
	if (len == 0)
	    break;

	argv[argc] = p;
	p += len + 1;
	argc++;
    }

    extern const char *__progname;
    __progname = argv[0];

    struct cobj_ref start_env_seg;
    int r = segment_lookup(start_env, &start_env_seg, 0, 0);
    if (r < 0 || r == 0)
	panic("libmain: cannot find start_env segment: %s", e2s(r));

    void *start_env_ro = (void *) USTARTENVRO;
    r = segment_map(start_env_seg, SEGMAP_READ, &start_env_ro, 0);
    if (r < 0)
	panic("libmain: cannot map start_env_ro: %s", e2s(r));

    struct cobj_ref tls = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    void *tls_va = (void *) UTLS;
    r = segment_map(tls, SEGMAP_READ | SEGMAP_WRITE, &tls_va, 0);
    if (r < 0)
	panic("libmain: cannot map tls: %s", e2s(r));

    tls_base = tls_va;
    tls_tidp = tls_base + PGSIZE - sizeof(uint64_t);
    tls_gate_args = tls_base + PGSIZE - sizeof(uint64_t) - sizeof(struct gate_call_data);
    assert(tls_gate_args == (void *) TLS_GATE_ARGS);
    tls_stack_top = tls_gate_args;

    fs_mtab = 0;
    r = segment_map(start_env->fs_mtab_seg, SEGMAP_READ, (void **) &fs_mtab, 0);
    if (r < 0)
	panic("libmain: cannot map mount table: %s", e2s(r));

    r = utrap_init();
    if (r < 0)
	panic("libmain: cannot setup utrap: %s", e2s(r));

    signal_init();
}

void
libmain(uint64_t arg0, uint64_t arg1)
{
    start_arg0 = arg0;
    start_arg1 = arg1;

    if (start_arg1 == 0)
	setup_env(start_arg0);

    exit(main(argc, &argv[0]));
}
