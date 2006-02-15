#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/memlayout.h>

extern int main(int argc, const char **argv);

uint64_t start_arg0;
uint64_t start_arg1;
start_env_t *start_env;

#define MAXARGS	16

void
libmain(uint64_t arg0, uint64_t arg1)
{
    int argc = 0;
    const char *argv[MAXARGS];

    memset(&argv, 0, sizeof(argv));

    start_arg0 = arg0;
    start_arg1 = arg1;

    if (start_arg1 == 0) {
	// This process has enough of an environment,
	// unlike a bootstrap process.
	start_env = (start_env_t *) start_arg0;

	const char *p = &start_env->args[0];
	for (int i = 0; i < MAXARGS; i++) {
	    size_t len = strlen(p);
	    if (len == 0)
		break;

	    argv[argc] = p;
	    p += len + 1;
	    argc++;
	}

	struct ulabel *l = label_get_current();
	assert(l);
	segment_set_default_label(l);

	struct cobj_ref start_env_seg;
	int r = segment_lookup(start_env, &start_env_seg, 0);
	if (r < 0)
	    panic("libmain: cannot find start_env segment: %s", e2s(r));

	void *start_env_ro = (void *) USTARTENVRO;
	r = segment_map(start_env_seg, SEGMAP_READ, &start_env_ro, 0);
	if (r < 0)
	    panic("libmain: cannot map start_env_ro: %s", e2s(r));
    }

    int r = main(argc, &argv[0]);

    exit(r);
}

void
exit(int r)
{
    close_all();

    if (start_env) {
	sys_segment_create(start_env->container, 0, 0, "dead flag");
	sys_obj_unref(COBJ(start_env->parent_container, start_env->container));
    }

    sys_thread_halt();
    panic("exit: still alive after sys_thread_halt");
}
