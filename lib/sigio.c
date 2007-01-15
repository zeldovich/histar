/*
 * This is a fairly deficient implementation of asynchronous
 * IO signal delivery (SIGIO), which only works for a single
 * file descriptor at a time, and doesn't properly pass file
 * descriptor ownership across fork/exec.  It also only does
 * read notifications.
 *
 * Hopefully this will be sufficient for gdbserver interrupt
 * handling code.
 */

#include <inc/fd.h>
#include <inc/sigio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

typedef union {
    struct {
	int32_t fd;
	uint32_t activated;
    };

    uint64_t v;
} jos_sigio_t;

static jos_sigio_t jos_sigio;
extern char *__progname;

static void __attribute__((noreturn))
jos_sigio_thread(void *arg)
{
    for (;;) {
	jos_sigio_t s = jos_sigio;
	if (s.fd == 0) {
	    sys_sync_wait(&jos_sigio.v, s.v, ~0ULL);
	    continue;
	}

	if (s.activated == 0) {
	    sys_sync_wait(&jos_sigio.v, s.v, ~0ULL);
	    continue;
	}

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s.fd, &fds);

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int r = select(s.fd + 1, &fds, 0, 0, &tv);
	if (r < 0) {
	    fprintf(stderr, "%s: jos_sigio_thread: select: %d (errno=%d)\n",
		    __progname, r, errno);
	    continue;
	}

	if (jos_sigio.v == s.v && FD_ISSET(s.fd, &fds)) {
	    jos_sigio.activated = 0;
	    kill(getpid(), SIGIO);
	}
    }
}

static int
jos_sigio_start_thread(void)
{
    static int started;

    if (started)
	return 0;

    struct cobj_ref tid;
    int r = thread_create(start_env->proc_container,
			  &jos_sigio_thread, 0,
			  &tid, "sigio_thread");
    if (r >= 0)
	started = 1;
    return r;
}

void
jos_sigio_enable(int fd)
{
    int r = jos_sigio_start_thread();
    if (r < 0) {
	fprintf(stderr, "%s: jos_sigio_enable: cannot start thread: %s\n",
		__progname, e2s(r));
	return;
    }

    if (jos_sigio.fd != 0) {
	fprintf(stderr, "%s: jos_sigio_enable: busy with fd %d, dropping fd %d\n",
		__progname, jos_sigio.fd, fd);
	return;
    }	

    jos_sigio.fd = fd;
    jos_sigio.activated = 1;
    sys_sync_wakeup(&jos_sigio.v);
}

void
jos_sigio_disable(int fd)
{
    if (fd == jos_sigio.fd) {
	jos_sigio.fd = 0;
	sys_sync_wakeup(&jos_sigio.v);
    }
}

void
jos_sigio_activate(int fd)
{
    if (fd == jos_sigio.fd) {
	jos_sigio.activated = 1;
	sys_sync_wakeup(&jos_sigio.v);
    }
}
