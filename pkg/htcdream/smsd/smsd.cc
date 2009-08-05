extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/smsd.h>
#include <inc/rild.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include "sms.h"

static int
gate_incoming_sms(struct smsd_req *req, struct smsd_reply *rep)
{
	struct incoming_sms_message ism;

	req->buf[sizeof(req->buf) - 1] = '\0';
	
	if (parse_sms_message(req->buf, &ism)) {
		fprintf(stderr, "smsd: malformed incoming sms message [%s]\n", req->buf);
		return -E_INVAL;
	} else { 
		printf("smsd: new incoming sms from [%s]: [%s]\n", ism.sender, ism.message);
		return 0;
	}

	//XXX- free the appropriate ism fields
}

static int
gate_outgoing_sms(struct smsd_req *req, struct smsd_reply *rep)
{
	req->buf[sizeof(req->buf) - 1] = '\0';
	req->buf2[sizeof(req->buf2) - 1] = '\0';

	char *number = req->buf;
	char *message = req->buf2;

	char *sms = generate_sms_message(number, message, "");	

	struct gate_call_data gcd;
	struct rild_req *rild_req = (struct rild_req *)&gcd.param_buf[0];
	struct rild_reply *rild_rep = (struct rild_reply *)&gcd.param_buf[0];

	int64_t rildgt = container_find(start_env->root_container, kobj_gate, "rild gate");
	if (rildgt < 0) {
		fprintf(stderr, "smsd: rild gate error, dropping outgoing sms message\n");
		return rildgt;
	}
	struct cobj_ref rildgate = COBJ(start_env->root_container, rildgt);

	strcpy(rild_req->buf, sms);
	rild_req->bufbytes = strlen(sms);
	rild_req->op = send_sms;
	gate_call(rildgate, 0, 0, 0).call(&gcd, 0);

	return rild_rep->err;
}

static void
smsd_dispatch(struct gate_call_data *parm)
{
	struct smsd_req *req = (struct smsd_req *) &parm->param_buf[0];
	struct smsd_reply *reply = (struct smsd_reply *) &parm->param_buf[0];

	static_assert(sizeof(*req) <= sizeof(parm->param_buf));
	static_assert(sizeof(*reply) <= sizeof(parm->param_buf));

	int err = 0;
	switch (req->op) {
	case incoming_sms:
		err = gate_incoming_sms(req, reply);
		break;

	case outgoing_sms:
		err = gate_outgoing_sms(req, reply);
		break;

	default:
		err = -E_INVAL;	
	}

	reply->err = err;
}

static void __attribute__((noreturn))
smsd_entry(uint64_t x, struct gate_call_data *parm, gatesrv_return *r)
{
	try {
		smsd_dispatch(parm);
	} catch (std::exception &e) {
		printf("%s: %s\n", __func__, e.what());
	}

	try {
		r->ret(0, 0, 0);
	} catch (std::exception &e) {
		printf("%s: ret: %s\n", __func__, e.what());
	}

	thread_halt();
}

int
main(int argc, char **argv)
{
	// init our gate
	struct cobj_ref g = gate_create(start_env->root_container,
	    "smsd gate", 0, 0, 0, &smsd_entry, 0);

	while(1)
		sleep(9999);
}
