extern "C" {
#include <inc/syscall.h>
}

#include <async.h>
#include <inc/labelutil.hh>
#include <dj/djgatesrv.hh>
#include <dj/djops.hh>

static void
gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    label vl, vc;
    thread_cur_verify(&vl, &vc);

    dj_outgoing_gate_msg m;
    djgate_incoming(gcd, vl, vc, &m, r);

    warn << "djdumpd: sender " << m.sender << "\n";
    warn << m.m;
}

int
main(int ac, char **av)
{
    label lpub(1);

    int64_t call_ct;
    error_check(call_ct = sys_container_alloc(start_env->shared_container,
					      lpub.to_ulabel(), "public call",
					      0, 65536));

    warn << "public call container: " << call_ct << "\n";

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djdumpd";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    warn << "djdumpd gate: " << g << "\n";

    thread_halt();
}
