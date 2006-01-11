#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>

static void __attribute__((noreturn))
thread_entry(void *arg)
{
    struct thread_args *ta = arg;

    ta->entry(ta->arg);

    // XXX need to unmap ta->stackbase
    // but how without using asm?
    // maybe jump to a static stack with a mutex around it..
    sys_obj_unref(ta->container);

    thread_halt();
}

int
thread_create(uint64_t container, void (*entry)(void*), void *arg,
	      struct cobj_ref *threadp, char *name)
{
    int64_t thread_ct = sys_container_alloc(container);
    if (thread_ct < 0)
	return thread_ct;

    struct cobj_ref tct = COBJ(container, thread_ct);
    sys_obj_set_name(tct, name);

    int stacksize = 2 * PGSIZE;
    struct cobj_ref stack;
    void *stackbase = 0;
    int r = segment_alloc(thread_ct, stacksize, &stack, &stackbase);
    if (r < 0) {
	sys_obj_unref(tct);
	return r;
    }

    sys_obj_set_name(stack, "thread stack");

    struct thread_args *ta = stackbase + stacksize - sizeof(*ta);
    ta->container = tct;
    ta->stackbase = stackbase;
    ta->entry = entry;
    ta->arg = arg;

    struct thread_entry e;
    r = sys_thread_get_as(&e.te_as);
    if (r < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return r;
    }

    e.te_entry = &thread_entry;
    e.te_stack = ta;
    e.te_arg = (uint64_t) ta;

    int64_t tid = sys_thread_create(thread_ct);
    if (tid < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return tid;
    }

    *threadp = COBJ(thread_ct, tid);
    sys_obj_set_name(*threadp, name);

    r = sys_thread_start(*threadp, &e);
    if (r < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return r;
    }

    return 0;
}

uint64_t
thread_id(void)
{
    int64_t tid = sys_thread_id();
    if (tid < 0)
	panic("sys_thread_id: %s", e2s(tid));
    return tid;
}

void
thread_halt(void)
{
    sys_thread_halt();
    panic("halt: still alive");
}

int
thread_get_label(uint64_t ctemp, struct ulabel *ul)
{
    uint64_t tid = thread_id();
    int r = sys_thread_addref(ctemp);
    if (r < 0)
	return r;

    r = sys_obj_get_label(COBJ(ctemp, tid), ul);
    sys_obj_unref(COBJ(ctemp, tid));
    return r;
}
