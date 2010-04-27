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
#include "../smdd/smd_rmnet.h"

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
	req->op = (_op);							\
	gate_call g(smddgate, 0, 0, 0)

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
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
	if (rep->err)
		return rep->err;
	return (rep->fd);
}

static int
smddgate_xxx_close(int op, int n)
{
	GATECALL_SETUP(op);
	req->fd = n;
	g.call(&gcd, 0);
	return (rep->err);
}

static int
smddgate_xxx_read(int op, int n, void *buf, size_t s)
{
	GATECALL_SETUP(op);
	req->fd = n;
	req->bufbytes = MIN(s, sizeof(rep->buf));
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
	if (rep->err)
		return (void *)rep->err;
	return (rep->token);
}

int
smddgate_rpcrouter_destroy_local_endpoint(void *endpt)
{
	GATECALL_SETUP(rpcrouter_destroy_local_endpoint);
	req->token = endpt;
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
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
	g.call(&gcd, 0);
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

	void *va = 0;
	error_check(segment_alloc(g.call_ct(), PGSIZE, &req->obj, &va, 0,
	    "smddgate_rpc_read"));
	req->bufbytes = s;
	g.call(&gcd, 0);
	if (!rep->err)
		memcpy(buf, va, rep->bufbytes);
	segment_unmap(va);

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

	void *va = 0;
	error_check(segment_alloc(g.call_ct(), PGSIZE, &req->obj, &va, 0,
	    "smddgate_rpc_write"));
	req->bufbytes = s;
	memcpy(va, buf, s);
	g.call(&gcd, 0);
	segment_unmap(va);

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
	g.call(&gcd, 0);
	if (rep->err)
		return rep->err;
// XXX- lack of check
	memcpy(endpts, rep->buf, rep->bufbytes);
	return (rep->bufbytes / 4);
}

int
smddgate_rmnet_open(int which)
{
	GATECALL_SETUP(rmnet_open);
	req->fd = which;
	req->bufbytes = 0;
	g.call(&gcd, 0);
	return (rep->err);
}

int
smddgate_rmnet_config(int which, struct htc_netconfig *hnc)
{
	GATECALL_SETUP(rmnet_config);
	req->fd = which;
	req->bufbytes = sizeof(*hnc);
	g.call(&gcd, 0);
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

	GATECALL_SETUP(rmnet_tx);

	void *va = 0;
	error_check(segment_alloc(g.call_ct(), PGSIZE, &req->obj, &va, 0,
	    "smddgate_rmnet_tx"));

	req->fd = which;
	req->bufbytes = len;
	memcpy(va, buf, len);
	g.call(&gcd, 0);
	segment_unmap(va);	

	return (rep->err);
}

int
smddgate_rmnet_rx(int which, char *buf, size_t len)
{
	if (len > PGSIZE) {
		cprintf("%s: bogus\n", __func__);
		return -E_INVAL;
	}

	GATECALL_SETUP(rmnet_rx);

	void *va = 0;
	error_check(segment_alloc(g.call_ct(), PGSIZE, &req->obj, &va, 0,
	    "smddgate_rmnet_rx"));

	req->fd = which;
	req->bufbytes = len;
	g.call(&gcd, 0);
	if (rep->err == 0)
		memcpy(buf, va, req->bufbytes);	//XXX danger
	segment_unmap(va);

	if (rep->err)
		return (rep->err);
	return (rep->bufbytes);
}

#if 0
int
smddgate_rmnet_fast_setup(int which, void **tx_ret, void **rx_ret)
{
	//GATECALL_SETUP(rmnet_fast_setup);

	struct gate_call_data gcd;
	struct smdd_req *req = (struct smdd_req *)&gcd.param_buf[0];
	struct smdd_reply *rep = (struct smdd_reply *)&gcd.param_buf[0];
	req->op = rmnet_fast_setup;
	label any(1);
	gate_call *g = new gate_call(smddgate, 0, &any, &any);
 
	*tx_ret = *rx_ret = 0;

	int len = sizeof(struct ringseg);
	int npages = (len + (PGSIZE - 1)) / PGSIZE;

	error_check(segment_alloc(g->call_ct(), npages * PGSIZE, &req->obj,
	    0, 0, "smddgate_rmnet_fast_tx"));
	error_check(segment_alloc(g->call_ct(), npages * PGSIZE, &req->obj2,
	    0, 0, "smddgate_rmnet_fast_rx"));

	error_check(sys_obj_set_fixedquota(req->obj));
	error_check(sys_obj_set_fixedquota(req->obj2));

	error_check(sys_segment_addref(req->obj,  start_env->proc_container));
	error_check(sys_segment_addref(req->obj2, start_env->proc_container));

	void *tx = 0;
	error_check(segment_map(
	    COBJ(start_env->proc_container, req->obj.object),
	    0, SEGMAP_READ | SEGMAP_WRITE, &tx, 0, 0));
	void *rx = 0;
	error_check(segment_map(
	    COBJ(start_env->proc_container, req->obj2.object),
	    0, SEGMAP_READ | SEGMAP_WRITE, &rx, 0, 0));

	req->fd = which;
	req->bufbytes  = len;
	req->bufbytes2 = len;
	g->call(&gcd, 0);

	if (rep->err) {
		segment_unmap(tx);
		segment_unmap(rx);
		cprintf("%s: SOMETHING FAILED!\n", __func__);
		return (rep->err);
	}

	cprintf("%s: allocated tx and rx segments (%d bytes each, %d pages)\n",
	    __func__, npages * PGSIZE, npages);

	*tx_ret = tx;
	*rx_ret = rx;

	return 0;
}
#else
#include <fcntl.h>
#include <sys/mman.h>
int
smddgate_rmnet_fast_setup(int which, void **tx_ret, void **rx_ret)
{
	GATECALL_SETUP(rmnet_fast_setup);
	(void)rep;

	int fd;

	fd = open("/tmp/rmnet-tx", O_CREAT | O_RDWR | O_TRUNC, 0777);
	ftruncate(fd, sizeof(struct ringseg));
	*tx_ret = mmap(0, sizeof(struct ringseg),  PROT_READ | PROT_WRITE, 0, fd, 0);

	fd = open("/tmp/rmnet-rx", O_CREAT | O_RDWR | O_TRUNC, 0777);
	ftruncate(fd, sizeof(struct ringseg));
	*rx_ret = mmap(0, sizeof(struct ringseg),  PROT_READ | PROT_WRITE, 0, fd, 0);

	cprintf("SMDDGATE mapped /tmp/rmnet-tx and /tmp/rmnet-rx @ %p and %p\n",
	    *tx_ret, *rx_ret);

	req->fd = which;
	req->bufbytes  = sizeof(struct ringseg);
	req->bufbytes2 = sizeof(struct ringseg);
	g.call(&gcd, 0);

	return 0;
}
#endif

} // extern C
