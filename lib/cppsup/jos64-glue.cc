#include <cstdlib>
extern "C" {
#include <inc/assert.h>
}

void
abort(void)
{
    panic("abort() called");
}
