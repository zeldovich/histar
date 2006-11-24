extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/debug_gate.h>
#include <inc/debug.h>

#include <machine/x86.h>

#include <unistd.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/user.h>
}

#include <inc/scopeguard.hh>
#include <inc/error.hh>

static const char ptrace_dbg = 0;

static void 
copy_to_utf(struct UTrapframe *u, struct user_regs_struct *r)
{
#define REG_COPY(R) u->utf_r##R = r->r##R
    REG_COPY(ax);  REG_COPY(bx);  REG_COPY(cx);  REG_COPY(dx);
    REG_COPY(si);  REG_COPY(di);  REG_COPY(bp);  REG_COPY(sp);
    REG_COPY(8);   REG_COPY(9);   REG_COPY(10);  REG_COPY(11);
    REG_COPY(12);  REG_COPY(13);  REG_COPY(14);  REG_COPY(15);
    REG_COPY(ip);
#undef REG_COPY
    u->utf_rflags = r->eflags;
}

static void
copy_to_user_regs(struct user_regs_struct *r, struct UTrapframe *u)
{
#define REG_COPY(R) r->r##R = u->utf_r##R
    REG_COPY(ax);  REG_COPY(bx);  REG_COPY(cx);  REG_COPY(dx);
    REG_COPY(si);  REG_COPY(di);  REG_COPY(bp);  REG_COPY(sp);
    REG_COPY(8);   REG_COPY(9);   REG_COPY(10);  REG_COPY(11);
    REG_COPY(12);  REG_COPY(13);  REG_COPY(14);  REG_COPY(15);
    REG_COPY(ip);
#undef REG_COPY
    r->eflags = u->utf_rflags;
    //r->orig_rax = 
    //r->cs = 
    //r->ss = 
    //r->fs_base = 
    //r->gs_base =
    //r->ds = 
    //r->es = 
    //r->fs =
    //r->gs = 
}

long int
ptrace(enum __ptrace_request request, ...) __THROW
{
    pid_t pid;
    void *addr, *data;

    va_list ap;
    va_start(ap, request);
    pid = va_arg(ap, pid_t);
    addr = va_arg(ap, void*);
    data = va_arg(ap, void*);
    va_end(ap);

    debug_print(ptrace_dbg, "request %d", request);
    
    if (request == PTRACE_TRACEME) {
	debug_gate_trace_is(1);
	return 0;
    }
    
    uint64_t ct = pid;
    int64_t gate_id = container_find(ct, kobj_gate, "debug");
    if (gate_id < 0) {
	debug_print(ptrace_dbg, "couldn't find debug gate for %ld\n", pid);
	return 0;
    }
    
    struct debug_args args;
    
    switch (request) {
    case PTRACE_GETREGS: {
	args.op = da_getregs;
	debug_gate_send(COBJ(ct, gate_id), &args);
	if (args.ret < 0)
	    return args.ret;
	
	struct UTrapframe *utf = 0;
	error_check(segment_map(args.ret_cobj, 0, SEGMAP_READ, 
				(void **)&utf, 0, 0));
	scope_guard<int, void*> seg_unmap(segment_unmap, utf);
	scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, args.ret_cobj);
	copy_to_user_regs((struct user_regs_struct*)data, utf);
	return 0;
    }
    case PTRACE_GETFPREGS: {
	args.op = da_getfpregs;
	debug_gate_send(COBJ(ct, gate_id), &args);
	if (args.ret < 0)
	    return args.ret;
	
	struct user_fpregs_struct *fpregs = 0;
	error_check(segment_map(args.ret_cobj, 0, SEGMAP_READ, 
				(void **)&fpregs, 0, 0));
	scope_guard<int, void*> seg_unmap(segment_unmap, fpregs);
	scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, args.ret_cobj);
	memcpy(data, fpregs, sizeof(fpregs));
	return 0;
    }
    case PTRACE_SETREGS: {
	struct cobj_ref arg_seg;
	struct UTrapframe *utf = 0;
	int size = sizeof(*utf);
	error_check(segment_alloc(start_env->shared_container,
				  size, &arg_seg, (void **)&utf,
				  0, "regs segment"));
	scope_guard<int, void*> seg_unmap(segment_unmap, utf);
	scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, arg_seg);
	
	copy_to_utf(utf, (struct user_regs_struct *)data);
	args.op =  da_setregs;
	args.arg_cobj = arg_seg;
	debug_gate_send(COBJ(ct, gate_id), &args);
	return args.ret;
    }
    case PTRACE_SETFPREGS: {
	struct cobj_ref arg_seg;
	struct Fpregs *fpregs = 0;
	int size = sizeof(*fpregs);
	error_check(segment_alloc(start_env->shared_container,
				  size, &arg_seg, (void **)&fpregs,
				  0, "fpregs segment"));
	scope_guard<int, void*> seg_unmap(segment_unmap, fpregs);
	scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, arg_seg);
	
	memcpy(fpregs, data, sizeof(*fpregs));
	args.op =  da_setfpregs;
	args.arg_cobj = arg_seg;
	debug_gate_send(COBJ(ct, gate_id), &args);
	return args.ret;
    }
    case PTRACE_PEEKTEXT: {
	args.op = da_peektext;
	args.addr = (uint64_t)addr;
	debug_gate_send(COBJ(ct, gate_id), &args);
	if (args.ret < 0) 
	    cprintf("ptrace: peektext failure: %ld\n", args.ret);
	return args.ret_word;
    }
    case PTRACE_POKETEXT: {
	args.op = da_poketext;
	args.addr = (uint64_t)addr;
	args.word = (uint64_t)data;
	debug_gate_send(COBJ(ct, gate_id), &args);
	return args.ret;
    }
    case PTRACE_CONT:
	if (data) {
	    cprintf("ptrace: signal delivery not implemented\n");
	    return -1;
	}
	args.op = da_cont;
	debug_gate_send(COBJ(ct, gate_id), &args);
	return args.ret;
    case PTRACE_SINGLESTEP:
	args.op = da_singlestep;
	debug_gate_send(COBJ(ct, gate_id), &args);
	return args.ret;
    case PTRACE_KILL:
	return kill(pid, SIGKILL);
    default:
	cprintf("ptrace: unknown request %d\n", request);
	print_backtrace();
	set_enosys();
	return -1;
    }
}

