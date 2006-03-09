#include <inc/lib.h>

void
_exit(int rval)
{
    close_all();

    if (start_env)
	process_report_exit(rval);

    thread_halt();
}
