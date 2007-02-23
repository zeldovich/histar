extern "C" {
#include <inc/syscall.h>
}

#include <dj/djhistar.hh>

uint64_t
histar_token_factory::token()
{
    return sys_pstate_timestamp();
}
