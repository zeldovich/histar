extern "C" {
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/bipipe.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

static int print_stats = 0;
static int throttle = 0;
static int throttle_children = 0;
static int use_rootrs = 0;

static void usage(void) __attribute__((noreturn));

static void
usage()
{
    printf("usage: [-p] [-r] total_uW children_uW [prog_args [...]]\n");
    exit(1);
}

void
do_process(char **args, uint64_t delay)
{
    while (1) {
	pid_t pid = fork();
	error_check(pid);
	if (!pid) {
	    // child
	    printf("*** Starting %s\n", args[0]);
	    error_check(execvp(args[0], args));
	    return;
	}

	// sleep seems to be broken - doesn't return left correctly
	uint64_t start = sys_clock_nsec();
	uint64_t end;
	do {
	    sleep(delay);
	    end = sys_clock_nsec();
	} while (end - start < delay * 1000 * 1000 * 1000);
    }
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
    throttle_children = atoi(av[1]);
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

    uint64_t ctid = start_env->shared_container;
    label l(1);
    int64_t r;
    char name[1000];

    // --- wget resources ---
    // create bkrs
    sprintf(name, "bkrs_%d", throttle);
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
    cobj_ref bkrs = COBJ(ctid, r);

    error_check(r = sys_limit_create(ctid, srcrs, bkrs, l.to_ulabel(), "bklm"));
    cobj_ref bklm = COBJ(ctid, r);
    error_check(sys_limit_set_rate(bklm, LIMIT_TYPE_CONST, throttle));

    // --- wget resources ---
    // create rs0
    sprintf(name, "wget_%d", throttle_children);
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
    cobj_ref rs0 = COBJ(ctid, r);

    error_check(r = sys_limit_create(ctid, bkrs, rs0, l.to_ulabel(), "lm0"));
    cobj_ref lm0 = COBJ(ctid, r);
    error_check(sys_limit_set_rate(lm0, LIMIT_TYPE_CONST, throttle_children));

    // --- movemail resources ---
    // create rs1
    sprintf(name, "movemail_%d", throttle_children);
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
    cobj_ref rs1 = COBJ(ctid, r);

    error_check(r = sys_limit_create(ctid, bkrs, rs1, l.to_ulabel(), "lm1"));
    cobj_ref lm1 = COBJ(ctid, r);
    error_check(sys_limit_set_rate(lm1, LIMIT_TYPE_CONST, throttle_children));

    if (print_stats)
	sys_toggle_debug(1);

    char *wget_child_args[] = { "/bin/wget", "http://www.nytimes.com/services/xml/rss/nyt/World.xml", NULL };
    char *movemail_child_args[] = { "/bin/movemail", "-p", "pop://cintard@171.66.3.208:1010/", "/t", "filez", NULL };

    pid_t pid = fork();
    error_check(pid);
    if (!pid) {
	// child
	error_check(sys_self_set_active_reserve(rs0));
	do_process(wget_child_args, 7);
	return 0;
    }
    error_check(sys_self_set_active_reserve(rs1));
    do_process(movemail_child_args, 13);

    return 0;
} catch (std::exception &e) {
    printf("capwrap: %s\n", e.what());
}
