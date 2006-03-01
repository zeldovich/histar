#include <inc/gatesrv.hh>
#include <inc/gateparam.hh>

static void
admind_dispatch(struct gate_call_data *parm)
{
    printf("admind_dispatch\n");
}

static void __attribute__((noreturn))
admind_entry(void *x, struct gate_call_data *parm, gatesrv_return *r)
{
    try {
	admind_dispatch(parm);
    } catch (std::exception &e) {
	printf("admind_entry: %s\n", e.what());
    }

    try {
	label *cs = new label(LB_LEVEL_STAR);
	label *ds = new label(3);
	label *dr = new label(0);
	r->ret(cs, ds, dr);
    } catch (std::exception &e) {
	printf("admind_entry: ret: %s\n", e.what());
    }

    thread_halt();
}

int
main(int ac, char **av)
try
{
    if (ac != 2) {
	printf("Usage: %s adm-handle\n", av[0]);
	exit(-1);
    }

    uint64_t adm_handle;
    error_check(strtoull(av[1], 0, 10, &adm_handle));

    label send(LB_LEVEL_STAR);
    label recv(1);

    // for now, allow anyone to make admin calls.
    // the sticking point is how to tell the shell that you want to pass
    // { adm_handle:* } when running admctl, but not for other things..
    //recv.set(adm_handle, 0);

    gatesrv g(start_env->root_container, "admgate", &send, &recv);
    g.set_entry_container(start_env->container);
    g.set_entry_function(&admind_entry, 0);
    g.enable();

    thread_halt();
} catch (std::exception &e) {
    printf("admind: %s\n", e.what());
}
