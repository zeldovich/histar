extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/debug_gate.h>
#include <inc/debug.h>
#include <inc/memlayout.h>

#include <machine/x86.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <bits/signalgate.h>
#include <sys/user.h>



}

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

static const char debug_gate_enable = 1;
static const char debug_dbg = 1;

static char debug_gate_inited = 0;

static struct cobj_ref debug_gate_as = COBJ(0,0);
static struct cobj_ref gs = COBJ(0,0);

static struct
{
    uint64_t wait;
    char     signo;
    struct user_regs_struct regs;
    uint64_t gen;
} ptrace_info;

static void
debug_gate_map_code(struct cobj_ref as, void **code_start, uint64_t *code_off)
{
    enum { uas_size = 64 };
    struct u_segment_mapping uas_ents[uas_size];
    struct u_address_space uas;
    uas.size = uas_size;
    uas.ents = &uas_ents[0];
    
    error_check(sys_as_get(as, &uas));
    
    for (uint64_t i = 0; i < uas.nent; i++) {
	if (uas.ents[i].flags & SEGMAP_EXEC && 
	    (uint64_t)uas.ents[i].va == 0x400000) {
	    uint64_t start = uas.ents[i].start_page * PGSIZE;
	    uint64_t nbytes = uas.ents[i].num_pages * PGSIZE;
	    cobj_ref seg = uas.ents[i].segment;
	    void *va = 0;
	    error_check(segment_map(seg, start, SEGMAP_READ|SEGMAP_WRITE, 
				    (void**)&va, &nbytes, 0));
	    *code_start = va;
	    *code_off = (uint64_t)uas.ents[i].va;
	    return;
	}
    }
    throw basic_exception("unable to map code segment");
}

static void
debug_gate_wait(struct debug_args *da)
{
    da->ret = ptrace_info.signo;
    da->ret_gen = ptrace_info.gen;
}

static void
debug_gate_cont(struct debug_args *da)
{
    ptrace_info.signo = 0;
    error_check(sys_sync_wakeup(&ptrace_info.wait));
    da->ret = 0;
}

static void
debug_gate_getregs(struct debug_args *da)
{
    if (!ptrace_info.signo) {
	debug_print(debug_dbg, "process not stopped!?");
	da->ret = -1;
	return;
    }

    struct cobj_ref ret_seg;
    void *va = 0;
    if (da->op == da_getregs) {
	error_check(segment_alloc(start_env->shared_container,
				  sizeof(ptrace_info.regs),
				  &ret_seg, &va,
				  0, "regs segment"));
	memcpy(va, &ptrace_info.regs, sizeof(ptrace_info.regs));
	da->ret_cobj = ret_seg;
	da->ret = sizeof(ptrace_info.regs);
    } else {
	// XXX fp support
	// XXX proper error message
	da->ret = 0;
    }
    scope_guard<int, void*> seg_unmap(segment_unmap, va);
}

static void
debug_gate_peektext(struct debug_args *da)
{
    uint64_t *word = (uint64_t *)da->addr;
    da->ret_word = *word;
    da->ret = 0;
}

static void
debug_gate_poketext(struct debug_args *da)
{   
    try {
	char *code_start;
	uint64_t code_offset;
	debug_gate_map_code(debug_gate_as, (void **)&code_start, &code_offset);
	scope_guard<int, void*> seg_unmap(segment_unmap, code_start);
	uint64_t offset = da->addr - code_offset;
	uint64_t *addr = (uint64_t *) (code_start + offset);
	*addr = da->word;
	da->ret = 0;
    }
    catch (basic_exception e) {
	cprintf("debug_gate_poketext: unable to write word: %s\n", e.what());
	da->ret = -1;
    }

    //debug_print(debug_dbg, "word %lx to addr %lx\n", da->word, da->addr);
    //uint64_t *addr = (uint64_t *)da->addr;
    //*addr = da->word;
    //da->ret = 0;
}

static void
debug_gate_setregs(struct debug_args *da)
{
    // XXX proper error message
    da->ret = 0;
}

static void __attribute__ ((noreturn))
debug_gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    struct debug_args *da = (struct debug_args *) &gcd->param_buf[0];
    static_assert(sizeof(*da) <= sizeof(gcd->param_buf));
    
    try {
	switch(da->op) {
        case da_wait:
	    debug_gate_wait(da);
	    break;
        case da_cont:
	    debug_gate_cont(da);
	    break;
        case da_getregs:
        case da_getfpregs:
	    debug_gate_getregs(da);
	    break;
        case da_setregs:
        case da_setfpregs:
	    debug_gate_setregs(da);
	    break;
        case da_peektext:
	    debug_gate_peektext(da);
	    break;
        case da_poketext:
	    debug_gate_poketext(da);
	    break;
        default:
	    da->ret = -1;
	    cprintf("debug_gate_entry: unkown op %d", da->op);
	    break;
	}
    } catch (basic_exception e) {
	cprintf("debug_gate_entry: error on op %d: %s", da->op, e.what());
	// XXX proper error message
	da->ret = -1;
    }
    gr->ret(0, 0, 0);
}

void
debug_gate_close(void)
{
    memset(&ptrace_info, 0, sizeof(ptrace_info));
    if (gs.object) {
	sys_obj_unref(gs);
	gs.object = 0;
    }
}

void
debug_gate_reset(void)
{
    gs.object = 0;
    debug_gate_inited = 0;
    memset(&ptrace_info, 0, sizeof(ptrace_info));
}

void
debug_gate_as_is(struct cobj_ref as)
{
    debug_gate_as = as;
}

void 
debug_gate_init(void)
{
    if (!debug_gate_enable)
	return;
    
    if (debug_gate_inited) {
	cprintf("debug_gate_init: trying to init twice\n");
	return ;
    }
    debug_gate_inited = 1;
    
    try {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);

	struct cobj_ref cur_as;
	error_check(sys_self_get_as(&cur_as));
	debug_gate_as_is(cur_as);
	
	gs = gate_create(start_env->shared_container,"debug", &tl, 
			 &tc, &debug_gate_entry, 0);
    } catch (std::exception &e) {
	cprintf("signal_gate_create: %s\n", e.what());
    }
}

void
debug_gate_signal_stop(char signo, struct sigcontext *sc)
{
    // XXX if another process' thread gets a signal, we end up here.
    // In the case of gdb, we stall forever since the signaled thread 
    // will never return from wait...
#if 0
    uint64_t tid = thread_id();
    uint64_t ct = start_env->proc_container;
    int64_t nslots = sys_container_get_nslots(ct);
    for (int64_t i = 0; i < nslots ; i++) {
	int64_t id = sys_container_get_slot_id(ct, i);
	if (id < 0)
	    continue;
	if ((uint64_t)id == tid)
	    break;
    }
#endif

    if (!sc) 
	debug_print(debug_dbg, "null struct sigcontext");
    else {
        struct UTrapframe *u = &sc->sc_utf;
	struct user_regs_struct *r = &ptrace_info.regs;
	
	r->r15 = u->utf_r15;
	r->r14 = u->utf_r14;
	r->r13 = u->utf_r13;
	r->r12 = u->utf_r12;
	r->rbp = u->utf_rbp;
	r->rbx = u->utf_rbx;
	r->r11 = u->utf_r11;
	r->r10 = u->utf_r10;
	r->r9 = u->utf_r9;
	r->r8 = u->utf_r8;
	r->rax = u->utf_rax;
	r->rcx = u->utf_rcx;
	r->rdx = u->utf_rdx;
	r->rsi = u->utf_rsi;
	r->rdi = u->utf_rdi;
	//r->orig_rax = 
	r->rip = u->utf_rip;
	//r->cs = 
	r->eflags = u->utf_rflags;
	r->rsp = u->utf_rsp;
	//r->ss = 
	//r->fs_base = 
	//r->gs_base =
	//r->ds = 
	//r->es = 
	//r->fs =
	//r->gs = 
    }
    
    ptrace_info.signo = signo;
    ptrace_info.gen++;
    debug_print(debug_dbg, "signo %d, gen %ld", signo, ptrace_info.gen);
    
    cprintf("debug_gate_signal_stop: tid %ld, pid %ld, rsp %lx, rip %lx\n", 
	    thread_id(), getpid(), read_rsp(), (uint64_t)&debug_gate_signal_stop);

    sys_sync_wait(&ptrace_info.wait, 0, ~0L);
}

extern "C" int64_t
debug_gate_send(struct cobj_ref gate, struct debug_args *da)
{
    struct gate_call_data gcd;
    struct debug_args *dag = (struct debug_args *) &gcd.param_buf[0];
    memcpy(dag, da, sizeof(*dag));
    try {
	gate_call(gate, 0, 0, 0).call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("kill: gate_call: %s\n", e.what());
	errno = EPERM;
	return -1;
    }
    memcpy(da, dag, sizeof(*da));
    return da->ret;
}
