extern "C" {
#include <inc/lib.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

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
