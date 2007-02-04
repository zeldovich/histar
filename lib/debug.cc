extern "C" {
#include <inc/lib.h>
}

#include <inc/error.hh>

void
print_backtrace()
{
    basic_exception e("backtrace tid=%ld ct=%ld",
		      thread_id(), start_env->shared_container);
    e.force_backtrace();
    e.print_where();
}
