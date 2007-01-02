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
static const char debug_dbg = 0;

static char debug_gate_inited = 0;
static char debug_trace = 0;

static struct cobj_ref debug_gate_as = COBJ(0,0);
static struct cobj_ref gs = COBJ(0,0);

static struct debug_info *dinfo = 0;

static void
debug_gate_map_code(struct cobj_ref as, uint64_t addr, 
		    void **code_start, uint64_t *code_off)
{
    enum { uas_size = 64 };
    struct u_segment_mapping uas_ents[uas_size];
    struct u_address_space uas;
    uas.size = uas_size;
    uas.ents = &uas_ents[0];
    
    error_check(sys_as_get(as, &uas));
    
    for (uint64_t i = 0; i < uas.nent; i++) {
	uint64_t a = (uint64_t)uas.ents[i].va;
	uint64_t z = (uint64_t)uas.ents[i].va + 
	    (uas.ents[i].num_pages * PGSIZE );
	if (a <= addr && addr < z) {
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
    da->ret = dinfo->signo;
    da->ret_gen = dinfo->gen;
}

static void
debug_gate_cont(struct debug_args *da)
{
    dinfo->signo = 0;
    error_check(sys_sync_wakeup(&dinfo->wait));
    da->ret = 0;
}

static void
debug_gate_singlestep(struct debug_args *da)
{
    dinfo->utf.utf_rflags |= FL_TF;
    error_check(sys_sync_wakeup(&dinfo->wait));
    da->ret = 0;
}

static void
debug_gate_getregs(struct debug_args *da)
{
    struct cobj_ref ret_seg;
    struct UTrapframe *utf = 0;
    error_check(segment_alloc(start_env->shared_container,
			      sizeof(*utf),
			      &ret_seg, (void **)&utf,
			      0, "regs segment"));
    scope_guard<int, void*> seg_unmap(segment_unmap, utf);
    memcpy(utf, &dinfo->utf, sizeof(struct UTrapframe));
    
    da->ret_cobj = ret_seg;
    da->ret = 0;
}

static void
debug_gate_getfpregs(struct debug_args *da)
{
    struct cobj_ref ret_seg;
    struct Fpregs *fpregs = 0;
    error_check(segment_alloc(start_env->shared_container,
			      sizeof(*fpregs),
			      &ret_seg, (void **)&fpregs,
			      0, "fpregs segment"));
    scope_guard<int, void*> seg_unmap(segment_unmap, fpregs);
    memcpy(fpregs, &dinfo->fpregs, sizeof(*fpregs));
    da->ret_cobj = ret_seg;
    da->ret = 0;
}

static void
debug_gate_setregs(struct debug_args *da)
{
    struct UTrapframe *utf;
    error_check(segment_map(da->arg_cobj, 0, SEGMAP_READ, 
			    (void **)&utf, 0, 0));
    scope_guard<int, void*> seg_unmap(segment_unmap, utf);
    memcpy(&dinfo->utf, utf, sizeof(dinfo->utf));

    da->ret = 0;
}

static void
debug_gate_setfpregs(struct debug_args *da)
{
    struct Fpregs *fpregs;
    error_check(segment_map(da->arg_cobj, 0, SEGMAP_READ, 
			    (void **)&fpregs, 0, 0));
    scope_guard<int, void*> seg_unmap(segment_unmap, fpregs);
    memcpy(&dinfo->fpregs, fpregs, sizeof(dinfo->fpregs));
    da->ret = 0;
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
	debug_gate_map_code(debug_gate_as, da->addr, 
			    (void **)&code_start, &code_offset);
	scope_guard2<int, void*, int> 
	    seg_unmap(segment_unmap_delayed, code_start, 1);
	uint64_t offset = da->addr - code_offset;
	uint64_t *addr = (uint64_t *) (code_start + offset);
	*addr = da->word;
	da->ret = 0;
    }
    catch (basic_exception &e) {
	cprintf("debug_gate_poketext: unable to write word: %s\n", e.what());
	da->ret = -1;
    }
}

static void __attribute__ ((noreturn))
debug_gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    struct debug_args *da = (struct debug_args *) &gcd->param_buf[0];
    static_assert(sizeof(*da) <= sizeof(gcd->param_buf));

    if (!dinfo->signo && da->op != da_wait) {
	cprintf("debug_gate_entry: thread not trapped?!?\n");
	da->ret = -1;
	gr->ret(0, 0, 0);
    }

    try {
	switch(da->op) {
        case da_wait:
	    debug_gate_wait(da);
	    break;
        case da_cont:
	    debug_gate_cont(da);
	    break;
	case da_singlestep:
	    debug_gate_singlestep(da);
	    break;
        case da_getregs:
	    debug_gate_getregs(da);
	    break;
        case da_getfpregs:
	    debug_gate_getfpregs(da);
	    break;
        case da_setregs:
	    debug_gate_setregs(da);
	    break;
        case da_setfpregs:
	    debug_gate_setfpregs(da);
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
    } catch (basic_exception &e) {
	cprintf("debug_gate_entry: error on op %d: %s", da->op, e.what());
	// XXX proper error message
	da->ret = -1;
    }
    gr->ret(0, 0, 0);
}

void
debug_gate_close(void)
{
    memset(&dinfo, 0, sizeof(dinfo));
    if (gs.object) {
	sys_obj_unref(gs);
	gs.object = 0;
    }
}

void
debug_gate_reset(void)
{
    gs.object = 0;
    debug_gate_as.object = 0;
    // keep for now...
    //debug_trace = 0;
    debug_gate_inited = 0;
    memset(&dinfo, 0, sizeof(dinfo));
}

void
debug_gate_as_is(struct cobj_ref as)
{
    debug_gate_as = as;
}

extern "C" void 
debug_gate_trace_is(char b)
{
    debug_trace = b;
}

extern "C" char
debug_gate_trace(void)
{
    return debug_trace;
}

extern "C" void
debug_gate_breakpoint(void)
{
    breakpoint();
}

void 
debug_gate_init(void)
{
    if (!debug_gate_enable)
	return;
    
    if (debug_gate_inited) {
	cprintf("debug_gate_init: trying to init twice\n");
	return;
    }
    debug_gate_inited = 1;

    try {
	label tl, tc;
	// avoid calling functions that manipulate label cache
	get_label_retry(&tl, thread_get_label);
	get_label_retry(&tc, sys_self_get_clearance);

	// require user privileges for debug
	if (start_env->user_grant)
	    tc.set(start_env->user_grant, 0);

	struct cobj_ref cur_as;
	error_check(sys_self_get_as(&cur_as));
	debug_gate_as_is(cur_as);
	
	struct cobj_ref to;
	int r = segment_alloc(start_env->shared_container, sizeof(*dinfo), &to,
			      (void **)&dinfo, 0, "debug info");
	if (r < 0) {
	    cprintf("debug_gate_init: unable to alloc segment: %s\n", e2s(r));
	    return;
	}

	gs = gate_create(start_env->shared_container,"debug", &tl, 
			 &tc, &debug_gate_entry, 0);
    } catch (std::exception &e) {
	cprintf("signal_gate_create: %s\n", e.what());
    }
}

void
debug_gate_on_signal(char signo, struct sigcontext *sc)
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

    struct UTrapframe *utf = &sc->sc_utf;
    memcpy(&dinfo->utf, utf, sizeof(dinfo->utf));
    dinfo->utf.utf_rflags &= ~FL_TF;
    fxsave(&dinfo->fpregs);
    dinfo->signo = signo;
    dinfo->gen++;

    debug_print(debug_dbg, "signo %d, gen %ld", signo, dinfo->gen);

    //cprintf("debug_gate_signal_stop: tid %ld, pid %ld, rsp %lx\n", 
    //thread_id(), getpid(), read_rsp());

    sys_sync_wait(&dinfo->wait, 0, ~0L);
    dinfo->signo = 0;
    memcpy(utf, &dinfo->utf, sizeof(*utf));
    fxrstor(&dinfo->fpregs);
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
	return -1;
    }
    memcpy(da, dag, sizeof(*da));
    return da->ret;
}
