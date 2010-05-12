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

#include <sys/wait.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

static int print_stats = 0;

static void usage(void) __attribute__((noreturn));

static void
usage()
{
    printf("usage: [-p]\n");
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

	int status;
	waitpid(pid, &status, 0);
	printf("*** Child %s exited; sleeping %d seconds\n",
	    args[0], (int)delay);

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

    while ((ch = getopt(ac, av, "p")) != -1) {
	switch (ch) {
	case 'p':
	    print_stats = 1;
	    break;
	default:
	    usage();
	}
    }
    ac -= optind;
    av += optind;

    if (print_stats)
	sys_toggle_debug(1);

    const char *wget_child_args[] = { "/bin/wget", "http://www.nytimes.com/services/xml/rss/nyt/World.xml", NULL };
    const char *movemail_child_args[] = { "/bin/movemail", "-p", "pop://cintard@171.66.3.208:1010/", "/t", "filez", NULL };

    pid_t pid = fork();
    error_check(pid);
    if (!pid) {
	// child
	do_process((char **)wget_child_args, 60);
	return 0;
    }

    sleep(30); // get out of sync
    do_process((char **)movemail_child_args, 60);

    return 0;
} catch (std::exception &e) {
    printf("capwrap: %s\n", e.what());
}
