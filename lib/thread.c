#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/pthread.h>
#include <inc/stack.h>

static pthread_mutex_t tl_stack_map_mutex;
static void *tl_stack_base;

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

    struct cobj_ref tls = COBJ(kobject_id_thread_ct,
			       kobject_id_thread_sg);
    assert(0 == sys_segment_resize(tls, 1));
    stack_switch(ta->container.container, ta->container.object,
		 (uint64_t) ta->stackbase, 0,
		 tl_stack_base + PGSIZE, &thread_exit);
}

int
thread_create(uint64_t container, void (*entry)(void*), void *arg,
	      struct cobj_ref *threadp, char *name)
{
    int r = 0;

    if (tl_stack_base == 0) {
	pthread_mutex_lock(&tl_stack_map_mutex);

	if (tl_stack_base == 0) {
	    struct cobj_ref tls = COBJ(kobject_id_thread_ct,
				       kobject_id_thread_sg);
	    r = sys_segment_resize(tls, 1);
	    if (r == 0)
		r = segment_map(tls, SEGMAP_READ | SEGMAP_WRITE,
				&tl_stack_base, 0);
	}

	pthread_mutex_unlock(&tl_stack_map_mutex);
	if (r < 0) {
	    printf("thread_create: cannot map self-stack: %s\n", e2s(r));
	    return r;
	}
    }

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
    r = sys_thread_get_as(&e.te_as);
    if (r < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return r;
    }

    e.te_entry = &thread_entry;
    e.te_stack = ta;
    e.te_arg = (uint64_t) ta;

    int64_t tid = sys_thread_create(thread_ct, name);
    if (tid < 0) {
	segment_unmap(stackbase);
	sys_obj_unref(tct);
	return tid;
    }

    *threadp = COBJ(thread_ct, tid);
    r = sys_thread_start(*threadp, &e, 0);
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
thread_get_label(struct ulabel *ul)
{
    uint64_t ctemp = kobject_id_thread_ct;

    uint64_t tid = thread_id();
    int r = sys_thread_addref(ctemp);
    if (r < 0)
	return r;

    r = sys_obj_get_label(COBJ(ctemp, tid), ul);
    sys_obj_unref(COBJ(ctemp, tid));
    return r;
}
