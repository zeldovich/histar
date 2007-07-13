#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/sections.h>
#include <inc/atomic.h>

#include <archcall.h>
#include <longjmp.h>
#include <kern/time.h>
#include <kern/signal.h>

static char procdbg = 0;
static volatile jos_atomic64_t lind_signal_pending;

extern void schedule_tail(struct task_struct *prev);

/*
 * For now kernel_execve is first invoked when trying to run linuxrc, 
 * or /sbin/init.  We let the arch-specific code take over instead.
 * Some kernel features require kernel_execve during  init, but luckily 
 * we don't need any of them yet.
 */
int 
kernel_execve(char *filename, char * argv[], char * envp[])
{
    printk(KERN_INFO "kernel_execve: %s ignored, calling arch_exec...\n", 
	   filename);
    
    return arch_exec();
}

long
kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
    int pid;
    
    current->thread.request.u.thread.proc = fn;
    current->thread.request.u.thread.arg = arg;
    pid = do_fork(CLONE_VM | CLONE_UNTRACED | flags, 0,
		  &current->thread.regs, 0, NULL, NULL);
    if (pid < 0)
	panic("do_fork failed in kernel_thread, errno = %d", pid);

    if (procdbg)
	printk(KERN_INFO "kernel_thread: pid %d fn 0x%lx arg 0x%lx\n", 
	       pid, (unsigned long)fn, (unsigned long)arg);
    
    return pid;
}

void 
new_thread(void *stack, jmp_buf *buf, void (*handler)(void))
{
    (*buf)[0].JB_IP = (unsigned long) handler;
    (*buf)[0].JB_SP = (unsigned long) stack +
	(PAGE_SIZE << CONFIG_KERNEL_STACK_ORDER) - sizeof(void *);
}

void 
new_thread_handler(void)
{
    int (*fn)(void *), n;
    void *arg;
    
    if(current->thread.prev_sched != NULL)
	schedule_tail(current->thread.prev_sched);
    current->thread.prev_sched = NULL;
    
    fn = current->thread.request.u.thread.proc;
    arg = current->thread.request.u.thread.arg;

    if (procdbg) {
	printk("new_thread_handler: fn 0x%lx arg 0x%lx\n",
	       (unsigned long) fn, (unsigned long) arg);
    }    

    /* The return value is 1 if the kernel thread execs a process,
     * 0 if it just exits
     */
    n = arch_run_kernel_thread(fn, arg, &current->thread.exec_buf);
    if(n == 1){
	panic("new_thread_handler: no process support");
    }
    else do_exit(0);
}

void *
_switch_to(void *prev, void *next, void *last)
{
    struct task_struct *from = prev;
    struct task_struct *to = next;
    
    to->thread.prev_sched = from;

    current->thread.saved_task = NULL ;

    if (procdbg) {
	printk(KERN_INFO "_switch_to: %d -> %d (0x%lx) \n", 
	       from->pid, to->pid,to->thread.switch_buf->__rip);
    }

    if (LIND_SETJMP(&from->thread.switch_buf) == 0)
	LIND_LONGJMP(&to->thread.switch_buf, 1);
    
    return current->thread.prev_sched;
}

void 
start_idle_thread(void *stack, jmp_buf *switch_buf)
{
    (*switch_buf)[0].JB_IP = (unsigned long) new_thread_handler;
    (*switch_buf)[0].JB_SP = (unsigned long) stack +
	(PAGE_SIZE << CONFIG_KERNEL_STACK_ORDER) -
	sizeof(void *);
    LIND_LONGJMP(switch_buf, 1);
}

static void
init_thread_registers(struct lind_pt_regs *to)
{
    ;
}

int 
copy_thread(int nr, unsigned long clone_flags, unsigned long sp,
	    unsigned long stack_top, struct task_struct * p,
	    struct pt_regs *regs)
{
    
    p->thread = (struct thread_struct) INIT_THREAD;

    init_thread_registers(&p->thread.regs.regs);
    p->thread.request.u.thread = current->thread.request.u.thread;

    new_thread(task_stack_page(p), &p->thread.switch_buf,
	       new_thread_handler);
    
    return 0;
}

void (*sig_kcall_handler)(void);
extern uint64_t sys_clock_nsec(void);
extern int sys_sync_wait(volatile uint64_t *addr, uint64_t val, uint64_t ns);
extern int sys_sync_wakeup(volatile uint64_t *addr);

void
lutrap_kill(signal_t s)
{
    for (;;) {
	uint64_t signal_old = jos_atomic_read(&lind_signal_pending);
	uint64_t signal_new = signal_old | s;
	uint64_t was;

	if (signal_old == signal_new)
	    return;

	was = jos_atomic_compare_exchange64(&lind_signal_pending,
					    signal_old, signal_new);
	if (was == signal_old) {
	    sys_sync_wakeup(&jos_atomic_read(&lind_signal_pending));
	    return;
	}
    }
}

void
lind_signal_init(void)
{
}

void
cpu_idle(void)
{
    static uint64_t last_jiffies_nsec;
    static uint64_t nsec_per_jiffies = 1000000000 / HZ;
    uint64_t now;
    uint64_t signal_old;

    while (1) {
	if (need_resched())
	    schedule();

	now = sys_clock_nsec();
	if (now > last_jiffies_nsec + nsec_per_jiffies) {
	    last_jiffies_nsec = now;
	    lutrap_kill(SIGNAL_ALARM);
	}

	signal_old = jos_atomic_read(&lind_signal_pending);
	if (signal_old) {
	    while (jos_atomic_compare_exchange64(&lind_signal_pending,
						 signal_old, 0) != signal_old)
		signal_old = jos_atomic_read(&lind_signal_pending);

	    irq_enter();
	    if (signal_old & SIGNAL_ALARM)
		__do_IRQ(LIND_TIMER_IRQ);
	    if (signal_old & SIGNAL_ETH)
		__do_IRQ(LIND_ETH_IRQ);
	    if (signal_old & SIGNAL_NETD)
		__do_IRQ(LIND_NETD_IRQ);
	    irq_exit();

	    if (signal_old & SIGNAL_KCALL)
		sig_kcall_handler();
	}

	sys_sync_wait(&jos_atomic_read(&lind_signal_pending), 0,
		      last_jiffies_nsec + nsec_per_jiffies);
    }
}
