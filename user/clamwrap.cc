extern "C" {
#include <inc/syscall.h>
#include <inc/fd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

int
main(int ac, const char **av)
try
{
    if (!start_env->user_taint)
	throw basic_exception("Missing user taint handle -- not logged in?");

    int64_t clam_taint;
    error_check(clam_taint = handle_alloc());

    int fds[2];
    if (pipe(&fds[0]) < 0)
	throw basic_exception("cannot create pipe: %s", strerror(errno));

    int nullfd = open("/dev/null", O_RDONLY);
    if (nullfd < 0)
	throw basic_exception("cannot open /dev/null: %s", strerror(errno));

    label clear;
    thread_cur_clearance(&clear);
    clear.set(start_env->user_taint, 3);
    clear.set(clam_taint, 3);
    thread_set_clearance(&clear);

    label taint(0);
    taint.set(start_env->user_taint, 3);
    taint.set(clam_taint, 3);

    label taint_star(LB_LEVEL_STAR);
    taint_star.set(start_env->user_taint, 3);
    taint_star.set(clam_taint, 3);

    error_check(fd_make_public(fds[0], taint.to_ulabel()));
    error_check(fd_make_public(fds[1], taint.to_ulabel()));
    error_check(fd_make_public(nullfd, taint.to_ulabel()));

    fs_inode clamscan;
    error_check(fs_namei("/bin/clamscan", &clamscan));

    av[0] = "clamscan";
    child_process cp =
	spawn(start_env->shared_container, clamscan,
	      nullfd, fds[1], fds[1],
	      ac, &av[0],
	      0, 0,
	      &taint_star, 0, 0, &taint, 0);
    close(fds[1]);
    close(nullfd);

    char buf[512];
    for (;;) {
	ssize_t cc = read(fds[0], &buf[0], sizeof(buf));
	if (cc < 0)
	    throw basic_exception("cannot read: %s", strerror(errno));

	if (cc == 0)
	    break;

	write(1, &buf[0], cc);
    }
} catch (std::exception &e) {
    printf("clamwrap: %s\n", e.what());
}
