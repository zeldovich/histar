#include <inc/backtrace.h>
#include <inc/utrap.h>
#include <unwind.h>

#if defined(JOS_ARCH_i386)
#include <machine/x86.h>

/*
 * For some reason, _Unwind_Backtrace() does not work on i386..
 */
int
backtrace(void **tracebuf, int maxents)
{
    int idx = 0;
    uint32_t ebp = read_ebp();
    uint32_t eip = 0;

    while (idx < maxents && ebp > 0x1000) {
	uint32_t *ebpp = (uint32_t *) ebp;
	ebp = ebpp[0];
	eip = ebpp[1];

	tracebuf[idx++] = (void *) eip;

	if (eip == 2 + (uintptr_t) &utrap_chain_dwarf2) {
	    if (idx >= maxents)
		break;

	    struct UTrapframe *utf = ((void *) ebpp) + 8;
	    tracebuf[idx++] = (void *) utf->utf_eip;
	    ebp = utf->utf_ebp;
	}
    }
    return idx;
}

#else

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

#endif
