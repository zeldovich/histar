#include <machine/stackwrap.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <inc/setjmp.h>
#include <inc/error.h>

#define STACKWRAP_MAGIC	0xabcd9262deed1713

struct stackwrap_state {
    stackwrap_fn fn;
    void *fn_arg;

    void *stackbase;
    struct jmp_buf entry_cb;
    struct jmp_buf task_state;

    int alive;
    uint64_t magic;
};

struct stackwrap_state *
stackwrap_cur(void)
{
    void *rsp = (void *) read_rsp();
    void *base = ROUNDDOWN(rsp, PGSIZE);
    struct stackwrap_state *ss = (struct stackwrap_state *) base;

    assert(ss->magic == STACKWRAP_MAGIC);
    assert(ss->alive);

    return ss;
}

static void __attribute__((noreturn))
stackwrap_entry(void)
{
    struct stackwrap_state *ss = stackwrap_cur();

    ss->fn(ss->fn_arg);
    ss->alive = 0;
    longjmp(&ss->entry_cb, 1);
}

void
stackwrap_wakeup(struct stackwrap_state *ss)
{
    if (setjmp(&ss->entry_cb) == 0)
	longjmp(&ss->task_state, 1);

    if (ss->alive == 0)
	page_free(ss->stackbase);
}

void
stackwrap_sleep(struct stackwrap_state *ss)
{
    if (setjmp(&ss->task_state) == 0)
	longjmp(&ss->entry_cb, 1);
}

int
stackwrap_call(stackwrap_fn fn, void *fn_arg)
{
    void *stackbase;
    int r = page_alloc(&stackbase);
    if (r < 0)
	return r;

    struct stackwrap_state *ss = (struct stackwrap_state *) stackbase;
    ss->fn = fn;
    ss->fn_arg = fn_arg;
    ss->stackbase = stackbase;
    ss->alive = 1;
    ss->magic = STACKWRAP_MAGIC;
    ss->task_state.jb_rip = (uint64_t) &stackwrap_entry;
    ss->task_state.jb_rsp = (uint64_t) stackbase + PGSIZE;

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
stackwrap_disk_iov(disk_op op, struct iovec *iov_buf, int iov_cnt, uint64_t offset)
{
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
stackwrap_disk_io(disk_op op, void *buf, uint32_t count, uint64_t offset)
{
    struct iovec iov = { buf, count };
    return stackwrap_disk_iov(op, &iov, 1, offset);
}

// Locks

struct lock_waiter {
    struct stackwrap_state *ss;
    LIST_ENTRY(lock_waiter) link;
};

void
lock_init(struct lock *l)
{
	memset(l, 0, sizeof(*l)) ;	
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

