#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stack.h>
#include <inc/error.h>
#include <string.h>

static int
thread_cleanup_internal(uint64_t ct, uint64_t thr_id, uint64_t stack_id, void *stackbase)
{
    void *stacktop = stackbase + thread_stack_pages * PGSIZE;
    int r = segment_unmap_range(stackbase, stacktop, 1);
    if (r < 0)
	return r;

    sys_obj_unref(COBJ(ct, stack_id));
    sys_obj_unref(COBJ(ct, thr_id));
    return 0;
}

int
thread_cleanup(struct thread_args *ta)
{
    return thread_cleanup_internal(ta->container, ta->thread_id,
				   ta->stack_id, ta->stackbase);
}

static void __attribute__((noreturn))
thread_exit(uint64_t ct, uint64_t thr_id, uint64_t stack_id, void *stackbase)
{
    int r = thread_cleanup_internal(ct, thr_id, stack_id, stackbase);
    if (r < 0)
	cprintf("thread_exit: cannot cleanup: %s\n", e2s(r));

    thread_halt();
}

static void __attribute__((noreturn))
thread_entry(void *arg)
{
    struct thread_args *ta = arg;

    ta->entry(ta->arg);

    stack_switch(ta->container, ta->thread_id, ta->stack_id,
		 (uint64_t) ta->stackbase,
		 tls_stack_top, &thread_exit);
}

int
thread_create(uint64_t container, void (*entry)(void*), void *arg,
	      struct cobj_ref *threadp, const char *name)
{
    return thread_create_option(container, entry, arg, 0, threadp, 
				name, 0, 0);
}

int
thread_create_option(uint64_t container, void (*entry)(void*), 
		     void *arg, uint32_t size_arg,
		     struct cobj_ref *threadp, const char *name, 
		     struct thread_args *thargs, int options)
{
    int r = 0;

    void *stackbase = 0;
    uint64_t stack_reserve_bytes = thread_stack_pages * PGSIZE;
    r = segment_map(COBJ(0, 0), 0, SEGMAP_STACK | SEGMAP_RESERVE,
		    &stackbase, &stack_reserve_bytes, 0);
    if (r < 0)
	return r;
    void *stacktop = stackbase + stack_reserve_bytes;

    uint64_t stack_alloc_bytes = PGSIZE;
    struct cobj_ref stack;
    r = segment_alloc(container, stack_alloc_bytes, &stack, 0, 0, "thread stack");
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	return r;
    }

    void *stack_alloc_base = stacktop - stack_alloc_bytes;
    r = segment_map(stack, 0, SEGMAP_READ | SEGMAP_WRITE,
		    &stack_alloc_base, &stack_alloc_bytes,
		    SEG_MAPOPT_OVERLAP);
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	return r;
    }

    struct thread_args *ta;
    if (options & THREAD_OPT_ARGCOPY) {
	if (size_arg > (stack_alloc_bytes - sizeof(*ta)))
	    return -E_NO_SPACE;

	ta = stacktop - sizeof(*ta) - size_arg;
	ta->arg = ta->entry_args;
    } else {
	ta = stacktop - sizeof(*ta);
	ta->arg = arg;
    }

    ta->container = container;
    ta->stack_id = stack.object;
    ta->entry = entry;
    ta->stackbase = stackbase;
    ta->options = options;
    
    struct thread_entry e;
    r = sys_self_get_as(&e.te_as);
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	return r;
    }

    e.te_entry = &thread_entry;
    e.te_stack = ta;
    e.te_arg[0] = (uint64_t) ta;

    int64_t tid = sys_thread_create(container, name);
    if (tid < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	return tid;
    }

    struct cobj_ref tobj = COBJ(container, tid);
    ta->thread_id = tid;
    r = sys_container_move_quota(container, tid, thread_quota_slush);
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	sys_obj_unref(tobj);
	return r;
    }

    r = sys_obj_set_fixedquota(tobj);
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	sys_obj_unref(tobj);
	return r;
    }

    /*
     * Should happen before the thread starts, in case the thread
     * wants to access these variables when it starts running..
     */
    *threadp = tobj;
    if (thargs)
	*thargs = *ta;

    /*
     * Do this last, in case either threadp or thargs points to
     * the arg buffer we're about to copy onto the new thread stack.
     */
    if (options & THREAD_OPT_ARGCOPY)
	memcpy(ta->entry_args, arg, size_arg);

    r = sys_thread_start(tobj, &e, 0, 0);
    if (r < 0) {
	segment_unmap_range(stackbase, stacktop, 1);
	sys_obj_unref(stack);
	sys_obj_unref(tobj);
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
