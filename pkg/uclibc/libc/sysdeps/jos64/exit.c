#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/prof.h>

#include <signal.h>
#include <unistd.h>

void
process_exit(int64_t rval, int64_t signo)
{
    close_all();

    prof_print(1);

    if (start_env)
	process_report_exit(rval, signo);

    thread_halt();
}

void
_exit(int rval)
{
    process_exit(rval, 0);
}
