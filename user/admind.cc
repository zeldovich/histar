extern "C" {
#include <inc/admind.h>
#include <inc/syscall.h>
#include <inc/error.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateparam.hh>

static void
admind_dispatch(struct gate_call_data *parm)
{
    struct admind_req *req = (struct admind_req *) &parm->param_buf[0];
    struct admind_reply *reply = (struct admind_reply *) &parm->param_buf[0];

    static_assert(sizeof(*req) <= sizeof(parm->param_buf));
    static_assert(sizeof(*reply) <= sizeof(parm->param_buf));

    switch (req->op) {
    case admind_op_get_top:
	reply->err = -999;
	break;

    case admind_op_drop:
	reply->err = sys_obj_unref(req->obj);
	break;

    default:
	reply->err = -E_BAD_OP;
    }
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
	r->ret(0, 0, 0);
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
