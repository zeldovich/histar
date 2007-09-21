#include <inc/lib.h>
#include <inc/syscall.h>

#include <sys/types.h>
#include <signal.h>

#include <os-jos64/lutrap.h>
#include <archtype.h>
#include <archcall.h>
#include <linuxsyscall.h>
#include <linuxthread.h>

static int main_pid;

static struct {
    jthread_mutex_t mu;
    union {
	void (*fn)(uint64_t a, uint64_t b);
	uint64_t sync;;
    };
    uint64_t a;
    uint64_t b;
    int64_t r;
    char waiting;
} call;

uint64_t call_count;

static void
kernel_call_handler(void)
{
    linux_kill(main_pid, SIGUSR1);
}

int
kernel_call(void (*fn)(uint64_t a, uint64_t b), uint64_t a, uint64_t b)
{
    int r;

    if (!main_pid)
	return -1;

    jthread_mutex_lock(&call.mu);
    while (call.fn != 0) {
	uint64_t val = call.sync;
	call.waiting = 1;
	jthread_mutex_unlock(&call.mu);
	sys_sync_wait(&call.sync, val, NSEC_PER_SECOND);
	jthread_mutex_lock(&call.mu);
    }

    call.fn = fn;
    call.a = a;
    call.b = b;
    call.r = 1;
    jthread_mutex_unlock(&call.mu);

    lutrap_kill(SIGNAL_KCALL);
    /* wait for call to finish */
    sys_sync_wait((uint64_t *) &call.r, 1, UINT64(~0));
    
    jthread_mutex_lock(&call.mu);
    r = (int)call.r;
    call.fn = 0;
    call.a = 0;
    call.b = 0;
    if (call.waiting)
	sys_sync_wakeup(&call.sync);
    jthread_mutex_unlock(&call.mu);
    return r;
}

int
kernel_call_init(void)
{
    extern void (*sig_kcall_handler)(void);
    sig_kcall_handler = kernel_call_handler;
    main_pid = linux_getpid();
    
    for (;;) {
	linux_pause();
	linux_thread_flushsig();
	jthread_mutex_lock(&call.mu);
	if (call.fn) {
	    (*call.fn)(call.a, call.b);
	    call.r = 0;
	    sys_sync_wakeup((uint64_t *) &call.r);
	}
	jthread_mutex_unlock(&call.mu);
    }
}
