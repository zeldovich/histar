#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <archcall.h>
#include <longjmp.h>

#include <asm/param.h>

static signal_handler_t *handler;

long long
arch_nsec(void)
{
    long long nsec;
    struct timeval tv;
    if (gettimeofday(&tv, 0) < 0) {
	perror("arch_nsec: gettimeofday:");
	return 0;
    }
    
    nsec = ((tv.tv_sec * 1000000) + tv.tv_usec) * 1000;
    return nsec;
}

void
arch_write_stderr(const char *string, unsigned len)
{
    while (len > 0) {
	int r = write(2, string, len);
	if (r < 0) {
	    perror("arch_write_stderr: write:");
	    return;
	}
	len -= r;
	string +=r;
    }
}

int
arch_printf(const char *fmt, ...)
{
    va_list ap;
    int cnt;
    
    va_start(ap, fmt);
    cnt = vprintf(fmt, ap);
    va_end(ap);
    
    return cnt;
}

int 
arch_run_kernel_thread(int (*fn)(void *), void *arg, void **jmp_ptr)
{
    jmp_buf buf;
    int n;
    
    *jmp_ptr = &buf;
    n = LIND_SETJMP(&buf);
    if(n != 0)
	return n;
    (*fn)(arg);
    return 0;
}

void net_test(void);

int
arch_exec(void)
{
    net_test();
    /* should never return */
    return -1;
}

void
arch_sleep(unsigned long sec)
{
    struct timespec ts;
    ts.tv_sec = sec;
    ts.tv_nsec = 0;

    nanosleep(&ts, NULL);
}

void
arch_signal_handler(signal_handler_t *h)
{
    handler = h;
}

static void
sig_handler(int signum)
{
    int lind_signum;
    
    switch(signum) {
    case SIGALRM:
	lind_signum = SIGNAL_ALARM;
	break;
    case SIGIO:
	lind_signum = SIGNAL_ETH;
	break;
    default:
	fprintf(stderr, "sig_handler: unknown %d", signum);
	return;
    }

    if (handler)
	(*handler)(lind_signum);

    return;
}

void
arch_init(void)
{
    int usec = 1000000/HZ;
    int timer_type = ITIMER_REAL;
    struct itimerval interval = ((struct itimerval) { { 0, usec },
						      { 0, usec } });
    
    if(setitimer(timer_type, &interval, NULL) == -1) 
	perror("unable to setitimer:");

    assert(signal(SIGALRM, &sig_handler) != SIG_ERR);
    assert(signal(SIGIO, &sig_handler) != SIG_ERR);
    assert(signal(SIGUSR1, &sig_handler) != SIG_ERR);
}

void
arch_halt(int exit_code)
{
    fprintf(stderr, "\n");
    exit(exit_code);
}

int
arch_file_size(const char *pn, unsigned long *size)
{
    int r;
    struct stat buf;
    r = stat(pn, &buf);
    if (r < 0) {
	perror("unable to stat:");
	return -1;
    }
	
    *size = buf.st_size;
    return 0;
}

int
arch_file_read(const char *pn, void *buf, unsigned long bytes)
{
    char *b = (char *)buf;
    int n, fd;

    fd = open(pn, O_RDONLY);
    if (fd < 0) {
	perror("unable to open:");
	return -1;
    }
    
    while (bytes) {
	n = read(fd, b, bytes);
	if (n < 0) {
	    perror("unable to read:");
	    return -1;
	}
	bytes -= n;
	b += n;
    }

    return 0;
}
