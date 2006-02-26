extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>

void
thread_drop_star(uint64_t handle)
{
    try {
	label clear;
	thread_cur_clearance(&clear);
	clear.set(handle, clear.get_default());
	error_check(sys_thread_set_clearance(clear.to_ulabel()));

	label self;
	thread_cur_label(&self);
	self.set(handle, self.get_default());
	error_check(sys_thread_set_label(self.to_ulabel()));
    } catch (std::exception &e) {
	printf("thread_drop_star: %s\n", e.what());
    }
}

void
thread_cur_label(label *l)
{
    int r;
    do {
	r = thread_get_label(l->to_ulabel());
	if (r == -E_NO_SPACE)
	    l->grow();
	else if (r < 0)
	    throw error(r, "thread_get_label");
    } while (r == -E_NO_SPACE);
}

void
thread_cur_clearance(label *l)
{
    int r;
    do {
	r = sys_thread_get_clearance(l->to_ulabel());
	if (r == -E_NO_SPACE)
	    l->grow();
	else if (r < 0)
	    throw error(r, "sys_thread_get_clearance");
    } while (r == -E_NO_SPACE);
}
