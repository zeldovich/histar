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
}

#include <inc/selftaint.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static int self_taint_debug = 0;

static void __attribute__((noreturn))
taint_self_tls(int *rp, label *l, uint64_t taint_ct, struct jos_jmp_buf *back)
{
    int r = thread_set_label(l);
    if (r >= 0)
	taint_cow(taint_ct, COBJ(0, 0));
    *rp = r;
    jos_longjmp(back, 1);
}

void
taint_self(label *taint)
{
    label cur_tl;
    thread_cur_label(&cur_tl);

    label ol;
    cur_tl.merge(taint, &ol, label::max, label::leq_starlo);
    ol.set(start_env->process_grant, 0);

    label tl;
    cur_tl.merge(taint, &tl, label::max, label::leq_starhi);

    if (self_taint_debug)
	cprintf("taint_self: taint label %s; ol %s, tl %s\n",
	        taint->to_string(), ol.to_string(), tl.to_string());

    // If the current thread label won't be able to write to
    // the new process container object label, we're acquiring
    // some non-trivial taint; try to report it.
    if (ol.compare(&cur_tl, label::leq_starhi) != 0)
	process_report_taint();

    int64_t taint_ct = sys_container_alloc(start_env->shared_container,
					   ol.to_ulabel(), "self-taint", 0, CT_QUOTA_INF);
    error_check(taint_ct);

    struct jos_jmp_buf back;
    int r;
    if (jos_setjmp(&back) == 0)
	stack_switch((uint64_t) &r, (uint64_t) &tl,
		     taint_ct, (uint64_t) &back,
		     tls_stack_top, (void *) &taint_self_tls);

    if (r < 0)
	throw error(r, "taint_self: cannot change label to %s",
		    tl.to_string());
}
