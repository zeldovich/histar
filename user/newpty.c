#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pty.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <inc/lib.h>
#include <inc/jthread.h>
#include <inc/fd.h>

static int64_t child_pid;

struct worker_args {
    int src_fd;
    int dst_fd;
};

static void
die(void)
{
    static jthread_mutex_t mu;
    jthread_mutex_lock(&mu);

    kill(child_pid, SIGHUP);
    exit(0);
}

static void __attribute__((noreturn))
worker(void *arg)
{
    struct worker_args *a = (struct worker_args *) arg;
    char buf[256];

    for (;;) {
	ssize_t cc = read(a->src_fd, &buf[0], sizeof(buf));
	if (cc <= 0)
	    die();

	write(a->dst_fd, &buf[0], cc);
    }
}

int
main(int ac, char **av)
{
    if (ac < 2) {
	printf("Usage: %s command [args..]\n", av[0]);
	exit(-1);
    }

    struct termios term, origterm;
    memset(&term, 0, sizeof(term));
    if (ioctl(0, TCGETS, &term) < 0)
	perror("TCGETS");

    struct winsize ws = { 0, 0, 0, 0 };
    if (ioctl(0, TIOCGWINSZ, &ws) < 0)
	perror("TIOCGWINSZ");

    int fdm, fds;
    if (openpty(&fdm, &fds, 0, &term, &ws) < 0) {
	perror("openpty");
	exit(-1);
    }

    memcpy(&origterm, &term, sizeof(term));
    term.c_iflag &= ~(IGNCR | ICRNL | INLCR);
    term.c_oflag &= ~(ONLCR);
    term.c_lflag &= ~(ECHO | ISIG | ICANON);
    ioctl(0, TCSETS, &term);

    struct Fd *fdseg_0, *fdseg_s;
    if ((fd_lookup(0,   &fdseg_0, 0, 0) >= 0) &&
	(fd_lookup(fds, &fdseg_s, 0, 0) >= 0) &&
	(fdseg_s->fd_dev_id == devpts.dev_id))
    {
	if (fdseg_0->fd_dev_id == devcons.dev_id)
	    fdseg_s->fd_pty.pty_cons_obj = fdseg_0->fd_cons.fbcons_seg;
	if (fdseg_0->fd_dev_id == devpts.dev_id)
	    fdseg_s->fd_pty.pty_cons_obj = fdseg_0->fd_pty.pty_cons_obj;
    }

    fcntl(fdm, F_SETFD, FD_CLOEXEC);
    child_pid = fork();
    if (child_pid < 0) {
	perror("fork");
	exit(-1);
    }

    if (child_pid == 0) {
	login_tty(fds);

	execv(av[1], &av[1]);
	perror("execv");
	exit(-1);
    }

    struct worker_args w1 = { .src_fd = 0, .dst_fd = fdm };
    struct worker_args w2 = { .src_fd = fdm, .dst_fd = 1 };
    struct cobj_ref t;
    thread_create(start_env->proc_container, &worker, &w1, &t, "w1");
    thread_create(start_env->proc_container, &worker, &w2, &t, "w2");

    int wstat;
    waitpid(child_pid, &wstat, 0);
    ioctl(0, TCSETS, &origterm);
    return 0;
}
