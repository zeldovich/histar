extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <stdio.h>
}

#include <inc/backtracer.hh>

void
print_backtrace(int use_cprintf)
{
    backtracer bt;
    for (int i = 0; i < bt.backtracer_depth(); i++) {
	void *addr = bt.backtracer_addr(i);
	if (use_cprintf)
	    cprintf("  %p\n", addr);
	else
	    fprintf(stderr, "  %p\n", addr);
    }
}
