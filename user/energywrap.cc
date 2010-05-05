extern "C" {
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/bipipe.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

static int print_stats = 0;
static int throttle = 0;
static int use_rootrs = 0;

void __attribute__((noreturn))
sigchld_handler(int i)
{
    if (print_stats)
	sys_toggle_debug(1);
    exit(0);
}

static void usage(void) __attribute__((noreturn));

static void
usage()
{
    printf("usage: [-p] [-r] main_uW prog_path [prog_args [...]]\n");
    exit(1);
}

int
main(int ac, char **av)
try
{
    extern int optind;
    int ch;

    if (ac < 3)
	usage();

    while ((ch = getopt(ac, av, "pr")) != -1) {
	switch (ch) {
	case 'p':
	    print_stats = 1;
	    break;
	case 'r':
	    use_rootrs = 1;
	    break;
	default:
	    usage();
	}
    }
    ac -= optind;
    av += optind;

    if (ac < 1)
	usage();

    throttle = atoi(av[0]);
    ac--;
    av++;

    char *args[ac + 1];
    for (int i = 0; i < ac; i++)
	args[i] = (char *)av[i];
    args[ac] = NULL;

    printf("running app with %ld uW on %s reserve\n", throttle,
	(use_rootrs) ? "the root" : "my");
    printf("process container: %ld\n", start_env->proc_container);

    struct cobj_ref srcrs;
    if (use_rootrs) {
	// find the root reserve
	int64_t rsid = container_find(start_env->root_container, kobj_reserve,
	    "root_reserve");
	if (rsid < 0) {
	    perror("couldn't find root_reserve");
	    return rsid;
	}
	srcrs = COBJ(start_env->root_container, rsid);
    } else {
	if (sys_self_get_active_reserve(&srcrs) < 0) {
	    perror("failed to get my reserve");
	    return -1;
	}
    }

    // ACTUALLY - HAVE THE CHILD PLACE THE RESERVE IN ITS OWN CONTAINER IN SOME WAY
    // === create main cp ===
    // Moving this to procpool prevents crashes when parent exits before child
    // This isn't always writable to children processes, though
    // Perhaps we create our own pretty open container and stick it there
    uint64_t ctid = start_env->shared_container;
    //uint64_t ctid = start_env->process_pool

    // allocate a category to ensure only this process and the child can use it
    //uint64_t rs0_grant = handle_alloc(); // integrity
    //uint64_t rs0_taint = handle_alloc(); // secrecy

    // create a reserve at the thread's current label
    label l(1);
    //l.set(rs0_grant, 0);
    //l.set(rs0_taint, 3);
    //label l;
    //thread_cur_label(&l);
    int64_t r;
    char name[1000];
    sprintf(name, "wrapreserve_%s_%d\n", args[0], throttle);
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
    printf("New reserve is at %lu\n", r);
    cobj_ref rs0 = COBJ(ctid, r);

    // create a limit between the source and rs0
    error_check(r = sys_limit_create(ctid, srcrs, rs0, l.to_ulabel(), "wraplimit"));
    printf("New limit is at %lu\n", r);
    cobj_ref lm0 = COBJ(ctid, r);
    error_check(sys_limit_set_rate(lm0, LIMIT_TYPE_CONST, throttle));

    // === start subprocess ===

    signal(SIGCHLD, sigchld_handler);

    pid_t pid = fork();
    error_check(pid);
    if (!pid) {
	// child
	signal(SIGCHLD, SIG_DFL);
	error_check(sys_self_set_active_reserve(rs0));
	error_check(execvp(args[0], args));
	return 0;
    }

    if (print_stats)
	sys_toggle_debug(1);

    while (1) {
	usleep(1000000);
    }

    return 0;
} catch (std::exception &e) {
    printf("capwrap: %s\n", e.what());
}
