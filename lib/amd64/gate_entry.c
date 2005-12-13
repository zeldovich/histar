#include <machine/trapcodes.h>
#include <machine/atomic.h>
#include <inc/syscall_num.h>
#include <inc/container.h>
#include <inc/gate.h>

// XXX we assume a lot of syscalls succeed, and
// it would be nice if we at least panic()ed.

void
gate_entry_wrapper()
{
    __asm__ __volatile__(

	// gate_entry(struct u_gate_entry *ug, struct cobj_ref call_args_obj)
	".globl gate_entry\n"
	"gate_entry:\n"

	// Save initial arguments
	"movq	%%rdi, %%r13\n"		// ug
	"movq	%%rsi, %%r14\n"
	"movq	%%rdx, %%r15\n"

	// syscall(SYS_thread_addref, ug->container)
	"movq	%1, %%rdi\n"
	"movq	(%%r13), %%rsi\n"
	"int	%0\n"

	// atomic_compare_exchange(&ug->entry_stack_use, 0, 1)
	"1:\n"
	"movl	$1, %%ecx\n"
	"movl	$0, %%eax\n"
	ATOMIC_LOCK "cmpxchgl %%ecx, 8(%%r13)\n"
	"cmp	$0, %%eax\n"
	"je	2f\n"

	// sys_thread_yield()
	"movq	%2, %%rdi\n"
	"int	%0\n"
	"jmp	1b\n"
	"2:\n"

	// Restore arguments, jump to C
	"movq	%%r13, %%rdi\n"
	"movq	%%r14, %%rsi\n"
	"movq	%%r15, %%rdx\n"
	"jmp	gate_entry_locked\n"

	: // no output

	: "n" (T_SYSCALL),
	  "n" (SYS_thread_addref),
	  "n" (SYS_thread_yield)

	);
}

void
gate_return_wrapper()
{
    __asm__ __volatile__(

	// gate_return(struct u_gate_entry *ug,
	//	       struct cobj_ref return_gate,
	//	       struct cobj_ref return_arg);
	".globl gate_return\n"
	"gate_return:\n"

	// Save initial arguments
	"movq	%%rdi, %%r11\n"		// ug
	"movq	%%rsi, %%r12\n"
	"movq	%%rdx, %%r13\n"
	"movq	%%rcx, %%r14\n"
	"movq	%%r8,  %%r15\n"

	// atomic_set(&ug->entry_stack_use, 1)
	"movl	$0, 8(%%r11)\n"

	// tid = sys_thread_id()
	"movq	%1, %%rdi\n"
	"int	%0\n"

	// sys_obj_unref(ug->container, tid)
	"movq	%2, %%rdi\n"
	"movq	(%%r11), %%rsi\n"
	"movq	%%rax, %%rdx\n"
	"int	%0\n"

	// gate_enter
	"movq	%3, %%rdi\n"
	"movq	%%r12, %%rsi\n"
	"movq	%%r13, %%rdx\n"
	"movq	%%r14, %%rcx\n"
	"movq	%%r15, %%r9\n"
	"int	%0\n"

	// panic
	"movq	$0x33330000deadbeef, %%rbx\n"
	"movq	(%%rbx), %%rbx\n"

	: // no output

	: "n" (T_SYSCALL),
	  "n" (SYS_thread_id),
	  "n" (SYS_obj_unref),
	  "n" (SYS_gate_enter)

	);
}
