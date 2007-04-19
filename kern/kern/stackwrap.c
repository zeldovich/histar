#include <kern/stackwrap.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/part.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <inc/intmacro.h>

#define STACKWRAP_MAGIC	UINT64(0xabcd9262deed1713)

struct stackwrap_state {
    stackwrap_fn fn;
    uint64_t fn_arg[3];

    void *stackbase;
    struct jos_jmp_buf entry_cb;
    struct jos_jmp_buf task_state;

    int alive;
    uint64_t magic;
};

struct stackwrap_state *
stackwrap_cur(void)
{
    void *rsp = (void *) (uintptr_t) karch_get_sp();
    void *base = ROUNDDOWN(rsp, PGSIZE);
    struct stackwrap_state *ss = (struct stackwrap_state *) base;

    assert(ss->magic == STACKWRAP_MAGIC);
    assert(ss->alive);

    return ss;
}

static void __attribute__((noreturn, no_instrument_function))
stackwrap_entry(void)
{
    struct stackwrap_state *ss = stackwrap_cur();

    ss->fn(ss->fn_arg[0], ss->fn_arg[1], ss->fn_arg[2]);
    ss->alive = 0;
    jos_longjmp(&ss->entry_cb, 1);
}

void
stackwrap_wakeup(struct stackwrap_state *ss)
{
    if (jos_setjmp(&ss->entry_cb) == 0)
	jos_longjmp(&ss->task_state, 1);

    if (ss->alive == 0)
	page_free(ss->stackbase);
}

void
stackwrap_sleep(struct stackwrap_state *ss)
{
    if (jos_setjmp(&ss->task_state) == 0)
	jos_longjmp(&ss->entry_cb, 1);
}

int
stackwrap_call(stackwrap_fn fn, uint64_t fn_arg0, uint64_t fn_arg1, uint64_t fn_arg2)
{
    void *stackbase;
    int r = page_alloc(&stackbase);
    if (r < 0)
	return r;

    struct stackwrap_state *ss = (struct stackwrap_state *) stackbase;
    ss->fn = fn;
    ss->fn_arg[0] = fn_arg0;
    ss->fn_arg[1] = fn_arg1;
    ss->fn_arg[2] = fn_arg2;
    ss->stackbase = stackbase;
    ss->alive = 1;
    ss->magic = STACKWRAP_MAGIC;
    ss->task_state.jb_rip = (uint64_t) &stackwrap_entry;
    ss->task_state.jb_rsp = (uintptr_t) stackbase + PGSIZE;

    stackwrap_wakeup(ss);
    return 0;
}

// Blocking disk I/O support

struct disk_io_request {
    struct stackwrap_state *ss;
    disk_io_status status;
    LIST_ENTRY(disk_io_request) link;
};

static void
disk_io_cb(disk_io_status status, void *arg)
{
    struct disk_io_request *ds = (struct disk_io_request *) arg;
    ds->status = status;
    stackwrap_wakeup(ds->ss);
}

disk_io_status
stackwrap_disk_iov(disk_op op, struct part_desc *pd, struct kiovec *iov_buf, 
		   int iov_cnt, uint64_t offset)
{
    if (offset > pd->pd_size) {
	cprintf("stackwrap_disk_io: offset greater than partition size: "
		"%"PRIu64" > %"PRIu64"\n",
		offset, pd->pd_size);
	return disk_io_failure;
    }
    
    uint64_t size = 0;
    for (int i = 0; i < iov_cnt; i++)
	size += iov_buf[i].iov_len;
    
    if (pd->pd_size < offset + size) {
	cprintf("stackwrap_disk_io: not enough space in partition: "
		"%"PRIu64" < %"PRIu64"\n",
		pd->pd_size - offset, size);
	return disk_io_failure;
    }
    
    offset += pd->pd_offset;
    
    struct stackwrap_state *ss = stackwrap_cur();
    struct disk_io_request ds = { .ss = ss };

    static LIST_HEAD(disk_waiters_list, disk_io_request) disk_waiters;
    static bool_t disk_queue_full;

    for (;;) {
	int r = disk_io(op, iov_buf, iov_cnt, offset, &disk_io_cb, &ds);
	if (r == 0) {
	    stackwrap_sleep(ss);
	    break;
	} else if (r == -E_BUSY) {
	    LIST_INSERT_HEAD(&disk_waiters, &ds, link);
	    disk_queue_full = 1;
	    stackwrap_sleep(ss);
	} else if (r < 0) {
	    cprintf("stackwrap_disk_io: unexpected error: %s\n", e2s(r));
	    ds.status = disk_io_failure;
	    break;
	}
    }

    disk_queue_full = 0;
    while (!disk_queue_full && !LIST_EMPTY(&disk_waiters)) {
	struct disk_io_request *rq = LIST_FIRST(&disk_waiters);
	LIST_REMOVE(rq, link);
	stackwrap_wakeup(rq->ss);
    }

    return ds.status;
}

disk_io_status
stackwrap_disk_io(disk_op op, struct part_desc *pd, 
		  void *buf, uint32_t count, uint64_t offset)
{
    struct kiovec iov = { buf, count };
    return stackwrap_disk_iov(op, pd, &iov, 1, offset);
}

// Locks

struct lock_waiter {
    struct stackwrap_state *ss;
    LIST_ENTRY(lock_waiter) link;
};

void
lock_init(struct lock *l)
{
    memset(l, 0, sizeof(*l));
}

void
lock_acquire(struct lock *l)
{
    while (l->locked) {
        struct lock_waiter w;
        w.ss = stackwrap_cur();
        LIST_INSERT_HEAD(&l->waiters, &w, link);
        stackwrap_sleep(w.ss);
    }

    l->locked = 1;
}

int
lock_try_acquire(struct lock *l)
{
    if (l->locked)
	return -E_BUSY;

    l->locked = 1;
    return 0;
}

void
lock_release(struct lock *l)
{
    l->locked = 0;
    struct lock_waiter *w = LIST_FIRST(&l->waiters);
    if (w == 0)
	return;
    LIST_REMOVE(w, link);
    stackwrap_wakeup(w->ss);
}
