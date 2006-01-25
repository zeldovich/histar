#include <machine/stackwrap.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <inc/setjmp.h>

#define STACKWRAP_MAGIC	0xabcd9262deed1713

struct stackwrap_state {
    stackwrap_fn fn;
    void *fn_arg;

    stackwrap_cb cb;
    void *cb_arg;

    void *stackbase;
    struct jmp_buf entry_cb;

    int alive;
    uint64_t magic;
};

static struct stackwrap_state *
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

    ss->cb(ss->cb_arg, ss->fn(ss->fn_arg));
    ss->alive = 0;
    longjmp(&ss->entry_cb, 1);
}

static void
stackwrap_check(struct stackwrap_state *ss)
{
    if (ss->alive == 0)
	page_free(ss->stackbase);
}

int
stackwrap_call(stackwrap_fn fn, void *fn_arg,
	       stackwrap_cb cb, void *cb_arg)
{
    void *stackbase;
    int r = page_alloc(&stackbase);
    if (r < 0)
	return r;

    struct stackwrap_state *ss = (struct stackwrap_state *) stackbase;
    ss->fn = fn;
    ss->fn_arg = fn_arg;
    ss->cb = cb;
    ss->cb_arg = cb_arg;
    ss->stackbase = stackbase;
    ss->alive = 1;
    ss->magic = STACKWRAP_MAGIC;

    if (setjmp(&ss->entry_cb) != 0) {
	stackwrap_check(ss);
	return 0;
    }

    struct jmp_buf jb;
    jb.jb_rip = (uint64_t) &stackwrap_entry;
    jb.jb_rsp = (uint64_t) stackbase + PGSIZE;
    longjmp(&jb, 1);
}

// Blocking disk I/O support

struct disk_io_state {
    struct stackwrap_state *ss;
    struct jmp_buf ret;
    disk_io_status status;
};

static void
disk_io_cb(disk_io_status status, void *b, uint32_t c, uint64_t o, void *arg)
{
    struct disk_io_state *ds = (struct disk_io_state *) arg;
    ds->status = status;

    if (setjmp(&ds->ss->entry_cb) != 0) {
	stackwrap_check(ds->ss);
	return;
    }

    longjmp(&ds->ret, 1);
}

disk_io_status
stackwrap_disk_io(disk_op op, void *buf, uint32_t count, uint64_t offset)
{
    struct stackwrap_state *ss = stackwrap_cur();
    struct disk_io_state ds = { .ss = ss };

    if (setjmp(&ds.ret) == 0) {
	disk_io(op, buf, count, offset, &disk_io_cb, &ds);
	longjmp(&ss->entry_cb, 1);
    }

    return ds.status;
}
