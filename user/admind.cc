extern "C" {
#include <inc/admind.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
}

#include <inc/gatesrv.hh>

static void __attribute__((noreturn))
admind_top(uint64_t ct, struct admind_reply *reply)
{
    throw error(-E_BAD_OP, "admind_top not implemented yet");
}

static void
admind_dispatch(struct gate_call_data *parm)
{
    struct admind_req *req = (struct admind_req *) &parm->param_buf[0];
    struct admind_reply *reply = (struct admind_reply *) &parm->param_buf[0];

    static_assert(sizeof(*req) <= sizeof(parm->param_buf));
    static_assert(sizeof(*reply) <= sizeof(parm->param_buf));

    int err = 0;
    try {
	switch (req->op) {
	case admind_op_get_top:
	    admind_top(req->obj.object, reply);
	    break;

	case admind_op_drop:
	    error_check(sys_obj_unref(req->obj));
	    break;

	default:
	    throw error(-E_BAD_OP, "unknown op %d", req->op);
	}
    } catch (error &e) {
	err = e.err();
	printf("admind_dispatch: %s\n", e.what());
    }
    reply->err = err;
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
    error_check(strtou64(av[1], 0, 10, &adm_handle));

    label send(LB_LEVEL_STAR);
    label recv(1);

    // for now, allow anyone to make admin calls.
    // the sticking point is how to tell the shell that you want to pass
    // { adm_handle:* } when running admctl, but not for other things..
    //recv.set(adm_handle, 0);

    struct cobj_ref g =
	gate_create(start_env->shared_container, "admgate", &send, &recv,
		    &admind_entry, 0);

    thread_halt();
} catch (std::exception &e) {
    printf("admind: %s\n", e.what());
}
