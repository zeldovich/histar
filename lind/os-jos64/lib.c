#include <machine/x86.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/stdio.h>
#include <inc/fs.h>
#include <inc/assert.h>
#include <inc/stack.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <archcall.h>
#include <longjmp.h>
#include <archenv.h>
#include <os-jos64/lutrap.h>
#include <os-jos64/netdev.h>

void main_loop(void);

long long
arch_nsec(void)
{
    return sys_clock_nsec();
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

int
arch_exec(void)
{
    main_loop();
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
arch_init(void)
{
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
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0) {
	cprintf("arch_file_size: fs_namei failed: %s\n", e2s(r));
	return -1;
    }
    
    r = fs_getsize(ino, size);
    if (r < 0) {
	cprintf("arch_file_size: fs_getsize failed: %s\n", e2s(r));
	return -1;
    }
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
