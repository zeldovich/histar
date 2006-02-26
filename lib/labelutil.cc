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
    struct ulabel *self = label_get_current();
    if (self == 0) {
	printf("thread_drop_star: cannot allocate label for cleanup\n");
	return;
    }
    scope_guard<void, struct ulabel *> self_free(label_free, self);

    int r = label_set_level(self, handle, self->ul_default, 1);
    if (r < 0) {
	printf("thread_drop_star: cannot reset handle %lu: %s\n", handle, e2s(r));
	return;
    }

    r = label_set_current(self);
    if (r < 0)
	printf("thread_drop_star: cannot change label: %s\n", e2s(r));
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
