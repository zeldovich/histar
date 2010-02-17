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

#include <machine/mmu.h>
#include <inc/syscall.h>
#include <inc/error.h>

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

struct tmpseg {
	void *ptr;
	int64_t ct;
	int64_t segid;
	struct cobj_ref ref;
};

static int
create_tmp_segment(const char *cname, const char *sname, size_t size, struct tmpseg *ts)
{
	label ct_label(1);
	error_check((ts->ct = sys_container_alloc(start_env->shared_container,
	    ct_label.to_ulabel(), cname, 0, CT_QUOTA_INF)));
	ts->segid = sys_segment_create(ts->ct, size, 0, sname);
	if (ts->segid < 0)
		return ts->segid;
	ts->ref = COBJ(ts->ct, ts->segid);
	error_check(segment_map(ts->ref, 0, SEGMAP_READ | SEGMAP_WRITE,
                            (void **)&ts->ptr, 0, 0));
	return 0;
}

static void
cleanup_tmp_segment(struct tmpseg *ts)
{
	segment_unmap(ts->ptr);
	sys_obj_unref(COBJ(ts->ct, ts->segid));
	sys_obj_unref(COBJ(start_env->shared_container, ts->ct));
}

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
smddgate_qmi_readwait(int *ns, int *rdys, int cnt)
{
	if (cnt < 0 || cnt > 3 || ns == NULL || rdys == NULL) {
		fprintf(stderr, "%s: bad args\n"); 
		return (E_INVAL);
	}

	GATECALL_SETUP(qmi_readwait);
	memcpy(&req->buf[0], &cnt, 4);
	memcpy(&req->buf[4], ns, 12);
	req->bufbytes = 16;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err)
		return (rep->err);
	memcpy(rdys, &rep->buf[0], 12);
	return (rep->err);
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

	if (s > PGSIZE) {
		cprintf("%s: WARNING: s > PGSIZE\n", __func__);
		return -E_INVAL;
	}

	struct tmpseg ts;
	error_check(create_tmp_segment("rpcbuf-r-ct", "rpcbuf-r", PGSIZE, &ts));

	req->obj = ts.ref;
	req->bufbytes = s;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (!rep->err)
		memcpy(buf, ts.ptr, rep->bufbytes);

	cleanup_tmp_segment(&ts);

	if (rep->err)
		return rep->err;
	return (rep->bufbytes);
}

int
smddgate_rpc_write(void *endpt, const void *buf, size_t s)
{
	GATECALL_SETUP(rpc_write);
	req->token = endpt;

	if (s > PGSIZE) {
		cprintf("%s: WARNING: s > PGSIZE\n", __func__);
		return -E_INVAL;
	}

	struct tmpseg ts;
	error_check(create_tmp_segment("rpcbuf-w-ct", "rpcbuf-w", PGSIZE, &ts));
	memcpy(ts.ptr, buf, s);

	req->obj = ts.ref;
	req->bufbytes = s;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);

	cleanup_tmp_segment(&ts);

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

int
smddgate_rmnet_open(int which)
{
	GATECALL_SETUP(rmnet_open);
	req->fd = which;
	req->bufbytes = 0;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	return (rep->err);
}

int
smddgate_rmnet_config(int which, struct htc_netconfig *hnc)
{
	GATECALL_SETUP(rmnet_config);
	req->fd = which;
	req->bufbytes = sizeof(*hnc);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	memcpy(hnc, &rep->netconfig, sizeof(*hnc));
	return (rep->err);
}

int
smddgate_rmnet_tx(int which, char *buf, size_t len)
{
	if (len > PGSIZE) {
		cprintf("%s: bogus\n", __func__);
		return -E_INVAL;
	}
cprintf("SMDDGATE_RMNET_TX: WANT TO TRANSMIT PACKET OF LEN %d\n", len);
	struct tmpseg ts;
	error_check(create_tmp_segment("rmnet-tx-ct", "rmnet_tx", PGSIZE, &ts));

	GATECALL_SETUP(rmnet_tx);
	req->fd = which;
	req->bufbytes = len;
	req->obj = ts.ref;
	memcpy(ts.ptr, buf, len);
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);

	cleanup_tmp_segment(&ts);

	return (rep->err);
}

int
smddgate_rmnet_rx(int which, char *buf, size_t len)
{
	if (len > PGSIZE) {
		cprintf("%s: bogus\n", __func__);
		return -E_INVAL;
	}
cprintf("SMDDGATE_RMNET_RX: WANT TO RECV UP TO %d\n", len);
	struct tmpseg ts;
	error_check(create_tmp_segment("rmnet-rx-ct", "rmnet_rx", PGSIZE, &ts));

	GATECALL_SETUP(rmnet_rx);
	req->fd = which;
	req->bufbytes = len;
	req->obj = ts.ref;
	gate_call(smddgate, 0, 0, 0).call(&gcd, 0);
	if (rep->err == 0)
		memcpy(buf, ts.ptr, req->bufbytes);	//XXX danger

	cleanup_tmp_segment(&ts);

	if (rep->err)
		return (rep->err);
	return (rep->bufbytes);
}
