extern "C" {
#include <machine/x86.h>
#include <inc/assert.h>
#include <inc/prof.h>
#include <inc/types.h>

#include <stdio.h>
#include <stdlib.h>

#include <unwind.h>
}

#include <inc/scopedprof.hh>

struct entry {
    void *func_addr;
    uint64_t cnt;
    uint64_t time;
};

static struct entry table[32];

static char enable = 0;

static _Unwind_Reason_Code
backtrace_cb(struct _Unwind_Context *ctx, void *arg)
{
    uint64_t *addr = (uint64_t *) arg; 
    if (*addr) {
	*addr = (uint64_t) _Unwind_GetIP(ctx);
	return _URC_END_OF_STACK;
    } else {
	*addr = 1;
	return _URC_NO_REASON;
    }
}

scoped_prof::scoped_prof(void) : func_addr_(0), start_(read_tsc())
{
    // watch out for inline
    _Unwind_Backtrace(&backtrace_cb, &func_addr_);
}

scoped_prof::~scoped_prof(void)
{
    uint64_t end = read_tsc();
    if (end < start_)
	prof_data(func_addr_, end + (~0UL - start_));
    else
	prof_data(func_addr_, end - start_);
}

void
prof_init(char on)
{
    if (on || getenv("JOS_PROF"))
	enable = 1;
}

void
prof_func(uint64_t time)
{
    uint64_t addr = 0;
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

    extern const char *__progname;
    const char *progn = "(unknown)";
    if (__progname)
	progn = __progname;

    if (use_cprintf)
	cprintf("prof_print: results for %s\n", progn);
    else
	printf("prof_print: results for %s\n", progn);
	
    for (uint64_t i = 0; i < sizeof(table) / sizeof(struct entry); i++) {
	if (table[i].func_addr) {
	    if (use_cprintf) 
		cprintf("%3ld addr %12lx cnt %12ld tot%12ld\n", 
			i, (uint64_t )table[i].func_addr, 
			table[i].cnt, table[i].time);
	    else 
		printf("%3ld addr %12lx cnt %12ld tot%12ld\n", 
		       i, (uint64_t )table[i].func_addr, 
		       table[i].cnt, table[i].time);
	}
    }
}
