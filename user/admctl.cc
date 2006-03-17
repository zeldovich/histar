extern "C" {
#include <inc/admind.h>
#include <inc/string.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/error.hh>

static void __attribute__((noreturn))
usage(const char *self)
{
    printf("Usage:\n");
    printf("    %s list ct\n", self);
    printf("    %s drop ct id\n", self);
    exit(-1);
}

int
main(int ac, char **av)
try
{
    if (ac < 2)
	usage(av[0]);

    int64_t admct = container_find(start_env->root_container,
				   kobj_container, "admind");
    error_check(admct);

    int64_t admgt = container_find(admct, kobj_gate, "admgate");
    error_check(admgt);
    struct cobj_ref admgate = COBJ(admct, admgt);

    struct gate_call_data gcd;
    struct admind_req *req = (struct admind_req *) &gcd.param_buf[0];
    struct admind_reply *reply = (struct admind_reply *) &gcd.param_buf[0];

    const char *op = av[1];
    if (!strcmp(op, "list")) {
	req->op = admind_op_get_top;
	gate_call(admgate, &gcd, 0, 0, 0);
	error_check(reply->err);
    } else if (!strcmp(op, "drop")) {
	uint64_t ct, id;
	if (ac != 4)
	    usage(av[0]);

	error_check(strtou64(av[2], 0, 10, &ct));
	error_check(strtou64(av[3], 0, 10, &id));

	req->op = admind_op_drop;
	req->obj = COBJ(ct, id);
	gate_call(admgate, &gcd, 0, 0, 0);
	error_check(reply->err);
    } else {
	usage(av[0]);
    }
} catch (std::exception &e) {
    printf("admctl: %s\n", e.what());
}
