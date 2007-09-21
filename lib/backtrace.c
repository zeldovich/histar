#include <inc/backtrace.h>
#include <unwind.h>

struct backtrace_state {
    void **tracebuf;
    int maxents;
    int idx;
};

static _Unwind_Reason_Code
backtrace_cb(struct _Unwind_Context *ctx, void *arg)
{
    struct backtrace_state *s = arg;

    if (s->idx >= s->maxents)
	return _URC_END_OF_STACK;
    s->tracebuf[s->idx++] = (void *) _Unwind_GetIP(ctx);
    return _URC_NO_REASON;
}

int
backtrace(void **tracebuf, int maxents)
{
    struct backtrace_state s = { tracebuf, maxents, 0 };
    _Unwind_Backtrace(&backtrace_cb, &s);
    return s.idx;
}
