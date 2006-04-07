extern "C" {
#include <inc/signal.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

void
segfault_helper(void *va)
{
    try {
	cobj_ref seg;
	uint64_t flags;
	error_check(segment_lookup(va, &seg, 0, &flags));

	char name[KOBJ_NAME_LEN];
	name[0] = '\0';
	sys_obj_get_name(seg, &name[0]);

	label seg_label;
	obj_get_label(seg, &seg_label);

	label cur_label;
	thread_cur_label(&cur_label);

	cprintf("segfault_helper: VA %p, segment %ld.%ld (%s), flags %lx\n",
		va, seg.container, seg.object, &name[0], flags);
	cprintf("segfault_helper: segment label %s, thread label %s\n",
		seg_label.to_string(), cur_label.to_string());
    } catch (std::exception &e) {
	cprintf("segfault_helper: %s\n", e.what());
    }
}
