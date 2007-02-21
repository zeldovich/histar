extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <async.h>
#include <inc/labelutil.hh>
#include <dj/djgatesrv.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>

int
main(int ac, char **av)
{
    label lpub(1);

    int64_t call_ct;
    error_check(call_ct = sys_container_alloc(start_env->shared_container,
					      lpub.to_ulabel(), "public call",
					      0, 10 * 1024 * 1024));

    warn << "djechod public container: " << call_ct << "\n";

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djechod";
    gd.func_ = (gatesrv_entry_t) &dj_rpc_srv;
    gd.arg_ = (void *) &dj_echo_service;

    cobj_ref g = gate_create(&gd);
    warn << "djechod gate: " << g << "\n";

    thread_halt();
}
