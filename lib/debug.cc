extern "C" {
#include <inc/lib.h>
}

#include <inc/error.hh>

void
print_backtrace()
{
    basic_exception("Backtrace requested").print_where();
}
