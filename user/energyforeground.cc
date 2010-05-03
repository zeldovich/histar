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
#include <inttypes.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

const uint64_t count = 2;

void
sigchld_handler(int i)
{
    static uint64_t quit_count = 0;
    quit_count++;
    if (quit_count == count) {
	sys_toggle_debug(1);
	exit(0);
    }
}

int
main(int ac, const char **av)
try
{
    if (ac < 4) {
	printf("usage: print_stats foremW backmW prog_path prog_args...\n");
	return -1;
    }
    const int print_stats = atoi(av[1]);
    const int forethrottle = atoi(av[2]);
    const int backthrottle = atoi(av[3]);

    char *args[ac - 4 + 1];
    for (int i = 0; i < ac - 4; i++)
	args[i] = (char *)av[4 + i];
    args[ac - 4] = NULL;

    // find the root reserve
    int64_t rsid = container_find(start_env->root_container, kobj_reserve, "root_reserve");
    if (rsid < 0) {
	perror("couldn't find root_reserve");
	return rsid;
    }
    struct cobj_ref rootrs = COBJ(start_env->root_container, rsid);

    uint64_t ctid = start_env->shared_container;

    label l(1);
    int64_t r;

    // ---- Create the reserve for foreground apps ----
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), "forereserve"));
    cobj_ref forers = COBJ(ctid, r);
    error_check(r = sys_limit_create(ctid, rootrs, forers, l.to_ulabel(), "forelimit"));
    cobj_ref forelm = COBJ(ctid, r);
    // Very high wattage
    error_check(sys_limit_set_rate(forelm, LIMIT_TYPE_CONST, forethrottle));

    // ---- Create the reserve for background apps ----
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), "backreserve"));
    cobj_ref backrs = COBJ(ctid, r);
    error_check(r = sys_limit_create(ctid, rootrs, backrs, l.to_ulabel(), "backlimit"));
    cobj_ref backlm = COBJ(ctid, r);
    error_check(sys_limit_set_rate(backlm, LIMIT_TYPE_CONST, backthrottle));

    // ---- Create the reserves for the bad process fork bomb ----
    cobj_ref rs0;
    cobj_ref rss[count];
    cobj_ref forelms[count];
    cobj_ref backlms[count];
    const size_t namel = 20;
    char name[namel];
    for (uint64_t i = 0; i < count; i++) {
	snprintf(&name[0], namel, "reserve%"PRIu64, i);
	error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
	rs0 = COBJ(ctid, r);
	rss[i] = rs0;

	// create a limit between the fore and rs0
	snprintf(&name[0], namel, "backlimit%"PRIu64, i);
	error_check(r = sys_limit_create(ctid, forers, rs0, l.to_ulabel(), name));
	cobj_ref lm0 = COBJ(ctid, r);
	forelms[i] = lm0;

	// create a limit between the back and rs0
	snprintf(&name[0], namel, "forelimit%"PRIu64, i);
	error_check(r = sys_limit_create(ctid, backrs, rs0, l.to_ulabel(), name));
	lm0 = COBJ(ctid, r);
	// set it's background rate
	error_check(sys_limit_set_rate(lm0, LIMIT_TYPE_CONST, backthrottle / 2));
	backlms[i] = lm0;
    }

    signal(SIGCHLD, sigchld_handler);

    for (uint64_t i = 0; i < count; i++) {
	// === start bad fork bomb 5 sec delay to each child ===
	pid_t pid = fork();
	error_check(pid);
	if (!pid) {
	    // child
	    signal(SIGCHLD, SIG_DFL);
	    error_check(sys_self_set_active_reserve(rss[i]));
	    error_check(execv(args[0], args));
	    return 0;
	}
    }

    // leave the wm running in the root_reserve
    if (print_stats)
	sys_toggle_debug(1);

    sleep(10);

    // 0 to foreground
    error_check(sys_limit_set_rate(forelms[0], LIMIT_TYPE_CONST, forethrottle));
    sleep(10);
    error_check(sys_limit_set_rate(forelms[0], LIMIT_TYPE_CONST, 0));

    sleep(10);

    // 1 to foreground
    error_check(sys_limit_set_rate(forelms[1], LIMIT_TYPE_CONST, forethrottle));
    sleep(10);
    error_check(sys_limit_set_rate(forelms[1], LIMIT_TYPE_CONST, 0));

    sleep(1200);

    return 0;
} catch (std::exception &e) {
    printf("energyforeground: %s\n", e.what());
}
