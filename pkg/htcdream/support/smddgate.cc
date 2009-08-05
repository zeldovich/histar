extern "C" {
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <inc/gateclnt.hh>
#include <inc/error.hh>

#include "../smdd/msm_rpcrouter2.h"
#include <inc/smdd.h>

#include "misc.h"
#include "smddgate.h"

static int64_t smddgt;
static struct cobj_ref smddgate;

#define GATECALL_SETUP(_op)							\
	struct gate_call_data gcd;						\
	struct smdd_req *req = (struct smdd_req *)&gcd.param_buf[0];		\
	struct smdd_reply *rep = (struct smdd_reply *)&gcd.param_buf[0];	\
	if (smddgt == 0) {							\
		cprintf("%s: smddgate == 0: smddgate_init not called yet!\n",	\
		    __func__);							\
		exit(1);							\
	}									\
	req->op = (_op)

int
smddgate_init()
{
	smddgt = container_find(start_env->root_container, kobj_gate, "smdd gate");
	error_check(smddgt);
	smddgate = COBJ(start_env->root_container, smddgt);
	return (0);
}

int
smddgate_get_battery_info(struct htc_get_batt_info_rep *batt_info)
{
	GATECALL_SETUP(get_battery_info);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
	memcpy(batt_info, &rep->batt_info, sizeof(*batt_info));
	return (0);
}

static int
smddgate_xxx_open(int op, int n)
{
	GATECALL_SETUP(op);
	req->fd = n;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
	return (rep->fd);
}

static int
smddgate_xxx_close(int op, int n)
{
	GATECALL_SETUP(op);
	req->fd = n;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	return (rep->err);
}

static int
smddgate_xxx_read(int op, int n, void *buf, size_t s)
{
	GATECALL_SETUP(op);
	req->fd = n;
	req->bufbytes = MIN(s, sizeof(rep->buf));
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
	memcpy(buf, rep->buf, rep->bufbytes);
	return (rep->bufbytes);
} 

static int
smddgate_xxx_write(int op, int n, const void *buf, size_t s)
{
	GATECALL_SETUP(op);
	req->fd = n;
	req->bufbytes = MIN(s, sizeof(req->buf));
if (s > sizeof(req->buf)) cprintf("%s: WARNING !!!! !!! !!! WRITE TOO BIG!\n", __func__);
	memcpy(req->buf, buf, req->bufbytes);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return (rep->err);
	return (rep->bufbytes);
}

int
smddgate_tty_open(int n)
{
	return (smddgate_xxx_open(tty_open, n));
}

int
smddgate_tty_close(int n)
{
	return (smddgate_xxx_close(tty_close, n));
}

int
smddgate_tty_read(int n, void *buf, size_t s)
{
	return (smddgate_xxx_read(tty_read, n, buf, s));
}

int
smddgate_tty_write(int n, const void *buf, size_t s)
{
	return (smddgate_xxx_write(tty_write, n, buf, s));
}

int
smddgate_qmi_open(int n)
{
	return (smddgate_xxx_open(qmi_open, n));
}

int
smddgate_qmi_close(int n)
{
	return (smddgate_xxx_close(qmi_close, n));
}

int
smddgate_qmi_read(int n, void *buf, size_t s)
{
	return (smddgate_xxx_read(qmi_read, n, buf, s));
}

int
smddgate_qmi_write(int n, const void *buf, size_t s)
{
	return (smddgate_xxx_write(qmi_write, n, buf, s));
}

int
smddgate_qmi_select()
{
	return (-1);
}

void *
smddgate_rpcrouter_create_local_endpoint(int is_userclient, uint32_t prog, uint32_t vers)
{
	GATECALL_SETUP(rpcrouter_create_local_endpoint);
	memcpy(&req->buf[0], &is_userclient, 4);
	memcpy(&req->buf[4], &prog, 4);
	memcpy(&req->buf[8], &vers, 4);
	req->bufbytes = 12;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return (void *)rep->err;
	return (rep->token);
}

int
smddgate_rpcrouter_destroy_local_endpoint(void *endpt)
{
	GATECALL_SETUP(rpcrouter_destroy_local_endpoint);
	req->token = endpt;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	return (rep->err);
}

int
smddgate_rpc_register_server(void *ept, uint32_t prog, uint32_t vers)
{
	GATECALL_SETUP(rpc_register_server);
	req->token = ept;
	memcpy(&req->buf[0], &prog, 4);
	memcpy(&req->buf[4], &vers, 4);
	req->bufbytes = 8;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	return (rep->err);
}

int
smddgate_rpc_unregister_server(void *ept, uint32_t prog, uint32_t vers)
{
	GATECALL_SETUP(rpc_unregister_server);
	req->token = ept;
	memcpy(&req->buf[0], &prog, 4);
	memcpy(&req->buf[4], &vers, 4);
	req->bufbytes = 8;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	return (rep->err);
}

int
smddgate_rpc_read(void *endpt, void *buf, size_t s)
{
	GATECALL_SETUP(rpc_read);
	req->token = endpt;
	req->bufbytes = MIN(s, sizeof(rep->buf));
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
	memcpy(buf, rep->buf, rep->bufbytes);
	return (rep->bufbytes);
}

int
smddgate_rpc_write(void *endpt, const void *buf, size_t s)
{
	GATECALL_SETUP(rpc_write);
	req->token = endpt;
	req->bufbytes = MIN(s, sizeof(req->buf));
if (s > sizeof(req->buf)) { cprintf("%s: WARNING !!!! !!! !!! WRITE TOO BIG!\n", __func__); return -1; }
	memcpy(req->buf, buf, req->bufbytes);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
	return (rep->bufbytes);
}

int
smddgate_rpc_endpoint_read_select(void **endpts, int nendpts, uint64_t timeout)
{
	GATECALL_SETUP(rpc_endpoint_read_select);
	req->bufbytes = 8 + 4 * nendpts;
	memcpy(&req->buf[0], &timeout, 8);
	memcpy(&req->buf[8], endpts, 4 * nendpts);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return rep->err;
// XXX- lack of check
	memcpy(endpts, rep->buf, rep->bufbytes);
	return (rep->bufbytes / 4);
}

}
