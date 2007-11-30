extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <stdio.h>
#include <dlfcn.h>
}

#include <inc/backtracer.hh>

void
print_backtrace(int use_cprintf)
{
    backtracer bt;
    for (int i = 0; i < bt.backtracer_depth(); i++) {
	void *addr = bt.backtracer_addr(i);

	const char *fn = "unknown";
	const char *sn = "unknown";
	void *off = 0;

	Dl_info dli;
	int r = dladdr(addr, &dli);
	if (r > 0) {
	    fn = dli.dli_fname;
	    sn = dli.dli_sname;
	    off = (void *) (((uintptr_t)addr) - ((uintptr_t)dli.dli_saddr));
	}

	if (use_cprintf)
	    cprintf("  %s:%s+%p (%p)\n", fn, sn, off, addr);
	else
	    fprintf(stderr, "  %s:%s+%p (%p)\n", fn, sn, off, addr);
    }
}

extern "C" int
__dummy_dladdr(const void *addr, Dl_info *info)
{
    return -1;
}

extern "C" __typeof(dladdr) dladdr
__attribute__((weak, alias ("__dummy_dladdr")));
