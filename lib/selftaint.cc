extern "C" {
#include <inc/fs.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <inc/stack.h>
#include <inc/memlayout.h>
#include <inc/setjmp.h>
#include <inc/taint.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#include <unistd.h>
#include <stdlib.h>
}

#include <inc/selftaint.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static int self_taint_debug = 0;

static void __attribute__((noreturn))
taint_self_tls(uint64_t rparg, uint64_t larg,
	       uint64_t taint_ct, uint64_t backarg)
{
    int *rp = (int *) (uintptr_t) rparg;
    label *l = (label *) (uintptr_t) larg;
    struct jos_jmp_buf *back = (struct jos_jmp_buf *) (uintptr_t) backarg;

    int r;
    r = sys_self_set_label(l->to_ulabel());
    if (r >= 0) {
	taint_cow(taint_ct, COBJ(0, 0));
	thread_label_cache_invalidate();
    }
    *rp = r;
    jos_longjmp(back, 1);
}

void
taint_self(label *taint)
{
    label cur_tl;
    thread_cur_label(&cur_tl);

    label new_tl(cur_tl);
    new_tl.add(*taint);

    if (self_taint_debug)
	cprintf("taint_self: taint label %s; new label %s\n",
	        taint->to_string(), new_tl.to_string());

    // If the current thread label won't be able to write to
    // the new process container object label, we're acquiring
    // some non-trivial taint; try to report it.
    if (!new_tl.can_flow_to(cur_tl))
	process_report_taint();

    int64_t taint_ct = sys_container_alloc(start_env->shared_container,
					   new_tl.to_ulabel(), "self-taint",
					   0, CT_QUOTA_INF);
    error_check(taint_ct);

    struct jos_jmp_buf back;
    int r;
    if (jos_setjmp(&back) == 0)
	stack_switch((uintptr_t) &r, (uintptr_t) &new_tl,
		     taint_ct, (uintptr_t) &back,
		     tls_stack_top, (void *) &taint_self_tls);

    if (r < 0)
	throw error(r, "taint_self: cannot change label to %s",
		    new_tl.to_string());
}
