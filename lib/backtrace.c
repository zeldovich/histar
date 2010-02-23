#include <inc/backtrace.h>
#include <inc/utrap.h>
#include <unwind.h>
#include <execinfo.h>

#if defined(JOS_ARCH_arm)

/*
 * fp -->   [ pc ]
 *          [ lr ]
 *          [ ip ]
 *          [ fp ]   --> ...
 */
static int
backtrace_common(void **tracebuf, int maxents, const uint32_t *fp)
{
	int i;
	for (i = 0; fp != 0 && *fp != 0 && i < maxents; i++) {
		tracebuf[i] = (void *)*fp;
		uint32_t *newfp = (uint32_t *)*(fp - 3);	
		if ((uintptr_t)newfp <= (uintptr_t)fp)
			fp = 0;
		else
			fp = newfp;
	}

	return i;
}

int
backtrace(void **tracebuf, int maxents)
{
	return backtrace_common(tracebuf, maxents, __builtin_frame_address(0));
}

int
backtrace_utf(void **tracebuf, int maxents, const struct UTrapframe *utf)
{
	if (utf == 0)
		return backtrace(tracebuf, maxents);
	return backtrace_common(tracebuf, maxents, (uint32_t *)utf->utf_fp);
}

#elif defined(JOS_ARCH_i386)
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

	    struct UTrapframe *lutf = ((void *) ebpp) + 8;
	    tracebuf[idx++] = (void *) lutf->utf_eip;
	    ebp = lutf->utf_ebp;
	}
    }
    return idx;
}

int
backtrace_utf(void **tracebuf, int maxents, const struct UTrapframe *utf)
{
	return backtrace(tracebuf, maxents);
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

int
backtrace_utf(void **tracebuf, int maxents, const struct UTrapframe *utf)
{
	return backtrace(tracebuf, maxents);
}

#endif

char **
backtrace_symbols(void *const *buffer, int size)
{
    return 0;
}

void
backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
}
