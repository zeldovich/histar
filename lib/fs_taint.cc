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

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static int fs_taint_debug = 0;

static void __attribute__((noreturn))
taint_tls(int *rp, struct ulabel *ul, struct jos_jmp_buf *back)
{
    *rp = 0;
    int r = sys_self_set_label(ul);
    if (r < 0) {
	*rp = r;
    } else {
	// XXX should actually allocate a container just
	// before we call sys_self_set_label..
	taint_cow(0);
    }
    jos_longjmp(back, 1);
}

int
fs_taint_self(struct fs_inode f)
try
{
    int r;
    label fl;
    do {
	r = sys_obj_get_label(f.obj, fl.to_ulabel());
	if (r == -E_NO_SPACE)
	    fl.grow();
	else if (r < 0)
	    throw error(r, "sys_obj_get_label");
    } while (r == -E_NO_SPACE);

    label tl;
    thread_cur_label(&tl);

    label tgt;
    tl.merge(&fl, &tgt, label::max, label::leq_starhi);

    if (fs_taint_debug)
	cprintf("fs_taint_self: file label %s, new label %s\n",
	       fl.to_string(), tgt.to_string());

    process_report_taint();

    struct jos_jmp_buf back;
    if (jos_setjmp(&back) == 0)
	stack_switch((uint64_t) &r, (uint64_t) tgt.to_ulabel(),
		     (uint64_t) &back, 0,
		     (char *) UTLS + PGSIZE, (void *) &taint_tls);

    return r;
} catch (error &e) {
    cprintf("fs_taint_self: %s\n", e.what());
    return e.err();
}
