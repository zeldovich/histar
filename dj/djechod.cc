extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <async.h>
#include <inc/labelutil.hh>
#include <dj/djgatesrv.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>

static void
gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    cobj_ref *djd_gate = (cobj_ref *) arg;
    dj_rpc_srv(dj_echo_service, *djd_gate, gcd, r);
}

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s djd-gate\n", av[0]);
	exit(-1);
    }

    cobj_ref djd_gate;
    djd_gate <<= av[1];

    label lpub(1);

    int64_t call_ct;
    error_check(call_ct = sys_container_alloc(start_env->shared_container,
					      lpub.to_ulabel(), "public call",
					      0, 10 * 1024 * 1024));

    warn << "djechod public container: " << call_ct << "\n";

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djechod";
    gd.func_ = &gate_entry;
    gd.arg_ = (void *) &djd_gate;

    cobj_ref g = gate_create(&gd);
    warn << "djechod gate: " << g << "\n";

    thread_halt();
}
