extern "C" {
#include <inc/lib.h>
}

#include <inc/error.hh>

void
print_backtrace()
{
    basic_exception("backtrace tid=%ld ct=%ld",
		    thread_id(), start_env->shared_container).print_where();
}
