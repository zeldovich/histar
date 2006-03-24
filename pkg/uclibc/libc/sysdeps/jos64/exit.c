#include <inc/lib.h>
#include <inc/fd.h>

#include <signal.h>
#include <unistd.h>

void
_exit(int rval)
{
    close_all();

    if (start_env)
	process_report_exit(rval);

    thread_halt();
}
