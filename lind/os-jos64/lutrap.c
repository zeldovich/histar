#include <machine/x86.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/stdio.h>
#include <inc/fs.h>
#include <inc/assert.h>
#include <inc/stack.h>
#include <inc/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <archcall.h>
#include <longjmp.h>
#include <archenv.h>

#define UTRAP_USER_TIMER 0x04
#define UTRAP_USER_ETH   0x08
#define UTRAP_USER_NETD  0x10
#define UTRAP_USER_KCALL 0x20

static signal_handler_t *handler;
static struct cobj_ref trap_th;
static struct cobj_ref trap_as;

static void __attribute__((noreturn, regparm(2)))
trap_onstack(struct UTrapframe *utf, signal_t s)
{
    if (handler)
	(*handler)(s);
    
    utrap_ret(utf);
}

static void
trap_handler(struct UTrapframe *utf)
{
    int trapsrc = utf->utf_trap_src;
    int trapno = utf->utf_trap_num;
    if (trapsrc == UTRAP_SRC_USER && 
	(trapno == UTRAP_USER_TIMER || 
	 trapno == UTRAP_USER_ETH ||
	 trapno == UTRAP_USER_NETD ||
	 trapno == UTRAP_USER_KCALL)) {
	signal_t sig;
	struct UTrapframe *utf_copy;
	utf_copy = (struct UTrapframe *) utf->utf_stackptr;
	utf_copy--;
	memcpy(utf_copy, utf, sizeof(*utf_copy));
	
	if (trapno == UTRAP_USER_TIMER)
	    sig = SIGNAL_ALARM;
	else if (trapno == UTRAP_USER_ETH)
	    sig = SIGNAL_ETH;
	else if (trapno == UTRAP_USER_NETD)
	    sig = SIGNAL_NETD;
	else if (trapno == UTRAP_USER_KCALL)
	    sig = SIGNAL_KCALL;

	else
	    panic("trap_handler");
	
	stack_switch((uint64_t)utf_copy, sig, 0, 0, 
		     utf_copy, trap_onstack);
    } else {
	signal_utrap(utf);
    }
}

static int
signal_to_trap(signal_t sig, uint32_t *trapno)
{
    switch(sig) {
    case SIGNAL_ALARM:
	*trapno = UTRAP_USER_TIMER;
	break;
    case SIGNAL_ETH:
	*trapno = UTRAP_USER_ETH;
	break;
    case SIGNAL_NETD:
	*trapno = UTRAP_USER_NETD;
	break;
    case SIGNAL_KCALL:
	*trapno = UTRAP_USER_KCALL;
	break;
    default:
	return -1;
    }
    return 0;
}

void
lutrap_signal(signal_handler_t *h)
{
    handler = h;
}

void
lutrap_kill(signal_t sig)
{
    int r;
    uint32_t trapno;

    r = signal_to_trap(sig, &trapno);
    if (r < 0)
	panic("lutrap_kill: unknown sig: %d", sig);
    
 top:
    r = sys_thread_trap(trap_th, trap_as, trapno, 0);
    if (r == -E_BUSY) {
	cprintf("lutrap_kill (%d): busy, retrying\n", trapno);
	usleep(100000);
	goto top;
    } else if (r < 0) {
	cprintf("lutrap_kill (%d): unable to trap Linux thread: %s\n", 
		trapno, e2s(r));
	cprintf("lutrap_kill (%d): halting\n", trapno);
	thread_halt();
    }
}

int
lutrap_init(void)
{
    trap_th = COBJ(start_env->proc_container, thread_id());
    sys_self_get_as(&trap_as);
    utrap_set_handler(trap_handler);
    return 0;
}