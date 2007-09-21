extern "C" {
#include <inc/assert.h>
#include <inc/prof.h>
#include <inc/types.h>
#include <inc/lib.h>
#include <inc/arch.h>

#include <stdio.h>
#include <stdlib.h>

#include <unwind.h>
#include <inttypes.h>
}

#include <inc/scopedprof.hh>

struct entry {
    void *func_addr;
    uint64_t cnt;
    uint64_t time;
};

static struct entry table[32];

static char enable = 0;
static uint64_t start_prof = 0;

static _Unwind_Reason_Code
backtrace_cb(struct _Unwind_Context *ctx, void *arg)
{
    uintptr_t *addr = (uintptr_t *) arg; 
    if (*addr) {
	*addr = (uintptr_t) _Unwind_GetIP(ctx);
	return _URC_END_OF_STACK;
    } else {
	*addr = 1;
	return _URC_NO_REASON;
    }
}

scoped_prof::scoped_prof(void) : func_addr_(0), start_(arch_read_tsc())
{
    // watch out for inline
    _Unwind_Backtrace(&backtrace_cb, &func_addr_);
}

scoped_prof::~scoped_prof(void)
{
    uint64_t end = arch_read_tsc();
    if (end < start_)
	prof_data(func_addr_, end + (UINT64(~0) - start_));
    else
	prof_data(func_addr_, end - start_);
}

void
prof_init(char on)
{
    if (on || getenv("JOS_PROF")) {
	enable = 1;
	start_prof = arch_read_tsc();
    }
}

void
prof_func(uint64_t time)
{
    uintptr_t addr = 0;
    // watch out for inline
    _Unwind_Backtrace(&backtrace_cb, &addr);
    prof_data((void *)addr, time);
}

void
prof_data(void *func_addr, uint64_t time)
{
    if (!enable)
	return;

    for (uint64_t i = 0; i < sizeof(table) / sizeof(struct entry); i++) {
	if (table[i].func_addr == func_addr ||
	    table[i].func_addr == 0) {
	    table[i].func_addr = func_addr;
	    table[i].time += time;
	    table[i].cnt++;
	    return;
	}
    }
    // oh well
}

void
prof_print(char use_cprintf)
{
    if (!enable || !table[0].func_addr)
	return;

    const char *progn = "(unknown)";
    if (jos_progname)
	progn = jos_progname;

    if (use_cprintf)
	cprintf("prof_print: results for %s\n", progn);
    else
	printf("prof_print: results for %s\n", progn);
	
    for (uint64_t i = 0; i < sizeof(table) / sizeof(struct entry); i++) {
	if (table[i].func_addr) {
	    if (use_cprintf) 
		cprintf("%3"PRIu64" addr %12p cnt %12"PRIu64" tot%12"PRIu64"\n", 
			i, table[i].func_addr, table[i].cnt, table[i].time);
	    else 
		printf("%3"PRIu64" addr %12p cnt %12"PRIu64" tot%12"PRIu64"\n", 
		       i, table[i].func_addr, table[i].cnt, table[i].time);
	}
    }

    uint64_t e = arch_read_tsc();
    uint64_t tot = 0;
    if (e < start_prof)
	tot = e + (UINT64(~0) - start_prof);
    else
	tot = e - start_prof;

    if (use_cprintf) 
	cprintf("%42s%12"PRIu64"\n", "tot", tot);
    else 
	printf("%42s%12"PRIu64"\n", "tot", tot);
}
