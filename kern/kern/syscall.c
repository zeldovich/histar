#include <kern/syscall.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <kern/sched.h>
#include <machine/thread.h>

static void
sys_cputs(const char *s)
{
    page_fault_mode = PFM_KILL;
    cprintf("%s", TRUP(s));
    page_fault_mode = PFM_NONE;
}

static void
sys_yield()
{
    schedule();
}

static void
sys_halt()
{
    thread_halt(cur_thread);
    schedule();
}

uint64_t
syscall(syscall_num num, uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5)
{
    switch (num) {
    case SYS_cputs:
	sys_cputs((const char*) a1);
	return 0;

    case SYS_yield:
	sys_yield();
	return 0;

    case SYS_halt:
	sys_halt();
	return 0;

    default:
	cprintf("Unknown syscall %d\n", num);
	return -E_INVAL;
    }
}
