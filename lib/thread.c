#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/pthread.h>
#include <inc/stack.h>

static void __attribute__((noreturn))
thread_exit(struct cobj_ref ct_obj, void *stackbase)
{
    segment_unmap(stackbase);
    sys_obj_unref(ct_obj);
    thread_halt();
}

static void __attribute__((noreturn))
thread_entry(void *arg)
{
    struct thread_args *ta = arg;

    ta->entry(ta->arg);

    stack_switch(ta->container.container, ta->container.object,
		 (uint64_t) ta->stackbase, 0,
		 tls_stack_top, &thread_exit);
}

int
thread_create(uint64_t container, void (*entry)(void*), void *arg,
	      struct cobj_ref *threadp, const char *name)
{
    int r = 0;

    int64_t thread_ct = sys_container_alloc(container, 0, name);
    if (thread_ct < 0)
	return thread_ct;

    struct cobj_ref tct = COBJ(container, thread_ct);

    int stacksize = 2 * PGSIZE;
    struct cobj_ref stack;
    void *stackbase = 0;
    r = segment_alloc(thread_ct, stacksize, &stack, &stackbase, 0, "thread stack");
    if (r < 0) {
	sys_obj_unref(tct);
	return r;
    }

    struct thread_args *ta = stackbase + stacksize - sizeof(*ta);
    ta->container = tct;
    ta->stackbase = stackbase;
    ta->entry = entry;
    ta->arg = arg;

    struct thread_entry e;
    r = sys_self_get_as(&e.te_as);
    if (r < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return r;
    }

    e.te_entry = &thread_entry;
    e.te_stack = ta;
    e.te_arg[0] = (uint64_t) ta;

    int64_t tid = sys_thread_create(thread_ct, name);
    if (tid < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return tid;
    }

    *threadp = COBJ(thread_ct, tid);
    r = sys_thread_start(*threadp, &e, 0, 0);
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
    if (tls_tidp) {
	uint64_t tls_tid = *tls_tidp;
	if (tls_tid)
	    return tls_tid;
    }

    int64_t tid = sys_self_id();
    if (tid < 0)
	panic("sys_self_id: %s", e2s(tid));

    if (tls_tidp)
	*tls_tidp = tid;
    return tid;
}

void
thread_halt(void)
{
    sys_self_halt();
    panic("halt: still alive");
}

int
thread_get_label(struct ulabel *ul)
{
    uint64_t tid = thread_id();

    return sys_obj_get_label(COBJ(0, tid), ul);
}

void
thread_sleep(uint64_t msec)
{
    int64_t cur = sys_clock_msec();
    if (cur < 0)
	panic("thread_sleep: cannot read clock: %s\n", e2s(cur));

    uint64_t v = 0xc0de;
    sys_sync_wait(&v, v, cur + msec);
}
