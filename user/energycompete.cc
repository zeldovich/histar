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

void __attribute((noreturn))
sigchld_handler(int i)
{
    exit(0);
}

void
badsigchld_handler(int i)
{
    static uint64_t quit_count = 0;
    quit_count++;
    if (quit_count == count)
	exit(0);
}

void
print_res_stats(struct cobj_ref rsref,
		uint64_t *last_nsec,
		uint64_t *last_level,
		int prefix)
{
    if (!*last_nsec)
	*last_nsec = sys_clock_nsec();

    ReserveInfo info;
    sys_reserve_get_info(rsref, &info);

    if (!*last_level)
	*last_level = info.rs_level;

    uint64_t elapsed;
    char name[256];

    printf("%d>reserve %ld:\n", prefix, rsref.object);
    sys_obj_get_name(rsref, &name[0]);
    printf("%d>  name    : %s\n", prefix, name);
    printf("%d>  level   : %ld\n", prefix, info.rs_level);
    printf("%d>  consumed: %ld\n", prefix, info.rs_consumed);
    printf("%d>  decayed : %ld\n", prefix, info.rs_decayed);

    elapsed = sys_clock_nsec() - *last_nsec;
    *last_nsec += elapsed;
    int64_t r = (info.rs_level - *last_level) * 1000000000 / elapsed;
    printf("%d>  est mW  : %ld\n", prefix, r);

    printf("%d>elapsed   : %lu\n", prefix, elapsed / 1000000);
    printf("%d>     ts   : %lu\n", prefix, *last_nsec);

    printf("\n");
}

int
main(int ac, const char **av)
try
{
    if (ac < 4) {
	printf("usage: print_stats main_mW prog_path prog_args...\n");
	return -1;
    }
    const int print_stats = atoi(av[1]);
    const int throttle = atoi(av[2]);

    char *args[ac - 3 + 1];
    for (int i = 0; i < ac - 3; i++)
	args[i] = (char *)av[3 + i];
    args[ac - 3] = NULL;

    printf("running app with %ld mW\n", throttle);
    printf("process container: %ld\n", start_env->proc_container);

    // TODO WANT TO CHANGE THIS TO sys_self_get_active_reserve
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

    // ---- Create the reserve for the good process ----
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), "goodreserve"));
    cobj_ref goodrs = COBJ(ctid, r);
    error_check(r = sys_limit_create(ctid, rootrs, goodrs, l.to_ulabel(), "goodlimit"));
    cobj_ref goodlm = COBJ(ctid, r);
    error_check(sys_limit_set_rate(goodlm, LIMIT_TYPE_CONST, throttle));

    // ---- Create the reserve for the bad process ----
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), "badreserve"));
    cobj_ref badrs = COBJ(ctid, r);
    error_check(r = sys_limit_create(ctid, rootrs, badrs, l.to_ulabel(), "badlimit"));
    cobj_ref badlm = COBJ(ctid, r);
    error_check(sys_limit_set_rate(badlm, LIMIT_TYPE_CONST, throttle));

    // ---- Create the reserves for the bad process fork bomb ----
    cobj_ref rs0;
    cobj_ref rss[count];
    cobj_ref lms[count];
    const size_t namel = 20;
    char name[namel];
    for (uint64_t i = 0; i < count; i++) {
	snprintf(&name[0], namel, "reserve%"PRIu64, i);
	error_check(r = sys_reserve_create(ctid, l.to_ulabel(), name));
	rs0 = COBJ(ctid, r);
	rss[i] = rs0;

	// create a limit between the badrs and rs0
	snprintf(&name[0], namel, "limit%"PRIu64, i);
	error_check(r = sys_limit_create(ctid, badrs, rs0, l.to_ulabel(), name));
	cobj_ref lm0 = COBJ(ctid, r);
	lms[i] = lm0;
    }

    signal(SIGCHLD, sigchld_handler);
    error_check(sys_self_set_active_reserve(goodrs));

    pid_t badpid = fork();
    if (!badpid) {
	// === start bad process ===
	signal(SIGCHLD, badsigchld_handler);
	error_check(sys_self_set_active_reserve(badrs));
	for (uint64_t i = 0; i < count; i++) {
	    // === start bad fork bomb 5 sec delay to each child ===
	    uint64_t last = sys_clock_nsec();
	    while (sys_clock_nsec() - last < 5000000000lu) {}
	    pid_t pid = fork();
	    error_check(pid);
	    if (!pid) {
		// child
		signal(SIGCHLD, SIG_DFL);
		// turn on the faucet
		error_check(sys_limit_set_rate(lms[i], LIMIT_TYPE_CONST, throttle / (2 * count)));
		error_check(sys_self_set_active_reserve(rss[i]));
		error_check(execv(args[0], args));
		return 0;
	    }
	}
	// spin the bad thread besides the hell its children raise
	for (;;);
	return 0;
    }

    uint64_t last_nsec = 0, last_level = 0;
    uint64_t glast_nsec = 0, glast_level = 0;
    uint64_t blast_nsec = 0, blast_level = 0;
    uint64_t last_nsecs[count], last_levels[count];
    for (uint64_t i = 0; i < count; i++) {
	last_nsecs[i] = 0;
	last_levels[i] = 0;
    }
    while (1) {
	uint64_t last = sys_clock_nsec();
	if (print_stats) {
	    print_res_stats(rootrs, &last_nsec, &last_level, 99);
	    print_res_stats(goodrs, &glast_nsec, &glast_level, 98);
	    print_res_stats(badrs, &blast_nsec, &blast_level, 97);
	    for (uint64_t i = 0; i < count; i++)
		print_res_stats(rss[i], &last_nsecs[i], &last_levels[i], i);
	}
	while (sys_clock_nsec() - last < 1000000000lu) {
	}
    }

    return 0;
} catch (std::exception &e) {
    printf("energycompete: %s\n", e.what());
}
