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

void __attribute__((noreturn))
sigchld_handler(int i)
{
    sys_toggle_debug(1);
    exit(0);
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
    error_check(r = sys_reserve_create(ctid, l.to_ulabel(), "wrapreserve"));
    printf("New reserve is at %lu\n", r);
    cobj_ref rs0 = COBJ(ctid, r);

    // create a limit between the root and rs0
    error_check(r = sys_limit_create(ctid, rootrs, rs0, l.to_ulabel(), "wraplimit"));
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
	error_check(execv(args[0], args));
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
