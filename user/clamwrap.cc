extern "C" {
#include <inc/syscall.h>
#include <inc/fd.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

int
main(int ac, char **av)
try
{
    if (!start_env->user_taint)
	throw basic_exception("Missing user taint handle -- not logged in?");

    int64_t clam_taint;
    error_check(clam_taint = handle_alloc());

    int fds[2];
    if (pipe(&fds[0]) < 0)
	throw basic_exception("cannot create pipe: %s", strerror(errno));

    label clear;
    thread_cur_clearance(&clear);
    clear.set(start_env->user_taint, 3);
    clear.set(clam_taint, 3);
    thread_set_clearance(&clear);

    label taint(0);
    taint.set(start_env->user_taint, 3);
    taint.set(clam_taint, 3);

    error_check(fd_make_public(fds[0], taint.to_ulabel()));
    error_check(fd_make_public(fds[1], taint.to_ulabel()));

    // ...

} catch (std::exception &e) {
    printf("clamwrap: %s\n", e.what());
}
