extern "C" {
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

#include <inc/gatesrv.hh>

#include "../support/misc.h"

#include "msm_smd.h"
#include "smd_tty.h"
#include "smd_qmi.h"
#include "smd_rmnet.h"
#include "smd_rpcrouter.h"
#include "msm_rpcrouter.h"
#include "msm_rpcrouter2.h"

#include <inc/smdd.h>

static struct msm_rpc_endpoint *battery_endpt;

static void
smdd_get_battery_info(struct smdd_req *request, struct smdd_reply *reply)
{
	struct rpc_request_hdr req;

	int rc = msm_rpc_call_reply(battery_endpt, 2, &req, sizeof(req),
	    &reply->batt_info, sizeof(reply->batt_info), 5000);
	if (rc < 0)
		fprintf(stderr, "%s: msm_rpc_call_reply failed: %d\n", __func__, rc);
	reply->err = rc;
}

static void
smdd_tty_open(struct smdd_req *request, struct smdd_reply *reply)
{
	reply->fd = smd_tty_open(request->fd);
	reply->err = (reply->fd < 0) ? reply->fd : 0;
}

static void
smdd_tty_read(struct smdd_req *request, struct smdd_reply *reply)
{
	request->bufbytes = MIN(request->bufbytes, (int)sizeof(reply->buf));
	reply->bufbytes = smd_tty_read(request->fd, (unsigned char *)reply->buf, request->bufbytes);
	reply->err = 0;
	if (reply->bufbytes < 0) {
		reply->err = reply->bufbytes;
		reply->bufbytes = 0;
	}
}

static void
smdd_tty_write(struct smdd_req *request, struct smdd_reply *reply)
{
	reply->bufbytes = smd_tty_write(request->fd, (unsigned char *)request->buf, request->bufbytes);
	reply->err = (reply->bufbytes < 0) ? reply->bufbytes : 0;
}

static void
smdd_tty_close(struct smdd_req *request, struct smdd_reply *reply)
{
	smd_tty_close(request->fd);
	reply->err = 0;
}

static void
smdd_qmi_open(struct smdd_req *request, struct smdd_reply *reply)
{
	reply->fd = smd_qmi_open(request->fd);
	reply->err = (reply->fd < 0) ? reply->fd : 0;
}

static void
smdd_qmi_read(struct smdd_req *request, struct smdd_reply *reply)
{
	request->bufbytes = MIN(request->bufbytes, (int)sizeof(reply->buf));
	reply->bufbytes = smd_qmi_read(request->fd, (unsigned char *)reply->buf, request->bufbytes);
	reply->err = 0;
	if (reply->bufbytes < 0) {
		reply->err = reply->bufbytes;
		reply->bufbytes = 0;
	}
}

static void
smdd_qmi_write(struct smdd_req *request, struct smdd_reply *reply)
{
	reply->bufbytes = smd_qmi_write(request->fd, (unsigned char *)request->buf, request->bufbytes);
	reply->err = (reply->bufbytes < 0) ? reply->bufbytes : 0;
}

static void
smdd_qmi_close(struct smdd_req *request, struct smdd_reply *reply)
{
	smd_qmi_close(request->fd);
	reply->err = 0;
}

static void
smdd_qmi_readwait(struct smdd_req *request, struct smdd_reply *reply)
{
	int cnt, ns[3], rdys[3];

	memcpy(&cnt, &request->buf[0], 4);
	memcpy(ns, &request->buf[4], 12);

	smd_qmi_readwait(ns, rdys, cnt);

	memcpy(&reply->buf[0], rdys, 12);
	reply->bufbytes = 12;
	reply->err = 0;
}

static void
smdd_rpcrouter_create_local_endpoint(struct smdd_req *request, struct smdd_reply *reply)
{
	int is_userclient;
	uint32_t prog, vers;

	if (request->bufbytes != 12) {
		reply->err = -E_INVAL;
		return;
	}

	memcpy(&is_userclient, &request->buf[0], 4);
	memcpy(&prog, &request->buf[4], 4);
	memcpy(&vers, &request->buf[8], 4);

	void *endpt = msm_rpcrouter_create_local_endpoint(is_userclient, prog, vers);
	if (IS_ERR(endpt)) {
		reply->err = PTR_ERR(endpt);
	} else {
		reply->err = 0;
		reply->token = endpt;
	}
}

static void
smdd_rpcrouter_destroy_local_endpoint(struct smdd_req *request, struct smdd_reply *reply)
{
	void *endpt = request->token;

	if (request->bufbytes != 4) {
		reply->err = -E_INVAL;
		return;
	}

	memcpy(&endpt, request->buf, 4);
//XXX- actually passing a pointer is _really_ dangerous and fragile. we should have a lookup to
//     ensure we don't get passed badness, even if we trust our client callers.
	reply->err = msm_rpcrouter_destroy_local_endpoint((struct msm_rpc_endpoint *)endpt);
}

static void
smdd_rpc_register_server(struct smdd_req *request, struct smdd_reply *reply)
{
	if (request->bufbytes != 8) {
		reply->err = -E_INVAL;
		return;
	}

	uint32_t prog, vers;
	memcpy(&prog, &request->buf[0], 4);
	memcpy(&vers, &request->buf[4], 4);
	reply->err = msm_rpc_register_server((struct msm_rpc_endpoint *)request->token, prog, vers);
}

static void
smdd_rpc_unregister_server(struct smdd_req *request, struct smdd_reply *reply)
{
	if (request->bufbytes != 8) {
		reply->err = -E_INVAL;
		return;
	}

	uint32_t prog, vers;
	memcpy(&prog, &request->buf[0], 4);
	memcpy(&vers, &request->buf[4], 4);
	reply->err = msm_rpc_unregister_server((struct msm_rpc_endpoint *)request->token, prog, vers);
}

static void
smdd_rpc_read(struct smdd_req *request, struct smdd_reply *reply)
{
	struct rr_fragment *frag, *next;

	void *ptr;
	if (segment_map(request->obj, 0, SEGMAP_READ | SEGMAP_WRITE,
                            (void **)&ptr, 0, 0) < 0) {
		reply->err = -E_INVAL;
		reply->bufbytes = 0;
		return;
	}

	reply->bufbytes = __msm_rpc_read((struct msm_rpc_endpoint *)request->token, &frag, request->bufbytes, -1);
	reply->err = 0;
	if (reply->bufbytes < 0) {
		reply->err = reply->bufbytes;
		reply->bufbytes = 0;
	}

	if (reply->err == 0) {
		char *buf = (char *)ptr;
		while (frag != NULL) {
			memcpy(buf, frag->data, frag->length);
			buf += frag->length;
			next = frag->next;
			free(frag);
			frag = next;
		}
	}

	segment_unmap(ptr);
}

static void
smdd_rpc_write(struct smdd_req *request, struct smdd_reply *reply)
{
	void *ptr;
cprintf("%s: MAPPING SEGMENT %lld.%lld\n", __func__, request->obj.container, request->obj.object);
	if (segment_map(request->obj, 0, SEGMAP_READ | SEGMAP_WRITE,
                            (void **)&ptr, 0, 0) < 0) {
cprintf("%s: FAILED TO MAP SEGMENT %lld.%lld\n", __func__, request->obj.container, request->obj.object);
		reply->err = -E_INVAL;
		reply->bufbytes = 0;
		return;
	}
cprintf("%s: MAPPED SEGMENT %lld.%lld\n", __func__, request->obj.container, request->obj.object);

	reply->bufbytes = msm_rpc_write((struct msm_rpc_endpoint *)request->token, ptr, request->bufbytes);
	segment_unmap(ptr);
	reply->err = (reply->bufbytes < 0) ? reply->bufbytes : 0;
}

static void
smdd_rpc_endpoint_read_select(struct smdd_req *request, struct smdd_reply *reply)
{
	// request->buf contains the timeout in the first 8 bytes, followed by
	// the endpoint pointers in each successive uint32_t.

	if (request->bufbytes < 12) {
		reply->err = -E_INVAL;
		reply->bufbytes = 0;
		return;
	}
	if ((request->bufbytes & 0x3) != 0) {
		reply->err = -E_INVAL;
		reply->bufbytes = 0;
		return;
	}

	int nendpts = (request->bufbytes - 8) / 4;
	uint64_t timeout;

	memcpy(&timeout, request->buf, 8);

	uint64_t *evtcnts = (uint64_t *)malloc(nendpts * sizeof(evtcnts[0]));
	if (evtcnts == NULL) {
		reply->err = -E_NO_MEM;
		reply->bufbytes = 0;
		return;
	}

	volatile uint64_t **addrs = (volatile uint64_t **)malloc(nendpts * sizeof(addrs[0]));
	if (addrs == NULL) { 
		free(evtcnts);
		reply->err = -E_NO_MEM;
		reply->bufbytes = 0;
		return;
	}

	void **rdyendpts = (void **)malloc(nendpts * sizeof(rdyendpts[0]));
	if (rdyendpts == NULL) { 
		free(evtcnts);
		free(addrs);
		reply->err = -E_NO_MEM;
		reply->bufbytes = 0;
		return;
	}

	int nready;
	while (1) {
		int idx = 0;

		nready = 0;
		for (int i = 0; i < nendpts; i++) { 
			struct msm_rpc_endpoint *ept;
			memcpy(&ept, &request->buf[8 + (i * 4)], 4); 
			if (ept == NULL) {
				cprintf("%s: warning - invalid endpt passed in\n", __func__);
			} else {
				pthread_mutex_lock(&ept->read_q_lock);
				if (!TAILQ_EMPTY(&ept->read_q)) {
					rdyendpts[nready++] = ept;
				} else {
					pthread_mutex_lock(&ept->waitq_mutex);
					evtcnts[idx] =  ept->waitq_evtcnt;
					addrs[idx++] = &ept->waitq_evtcnt;
					pthread_mutex_unlock(&ept->waitq_mutex);
				}
				pthread_mutex_unlock(&ept->read_q_lock);
			}
		}

		if (nready)
			break;

		// if no fds to check, optionally sleep and return 0
		if (idx == 0) {
			if (timeout != 0) {
				uint64_t now = sys_clock_nsec();
				if (now < timeout)
					usleep((timeout - now) / 1000);
			}

			reply->err = 0;
			reply->bufbytes = 0;
			goto out;
		}

		if (timeout != 0 && sys_clock_nsec() >= (int64_t)timeout) {
			reply->err = 0;
			reply->bufbytes = 0;
			goto out;
		}

		// we have >= 1 endpt to check and timeout is either infinity or in the future.
		sys_sync_wait_multi(addrs, evtcnts, NULL, idx, (timeout != 0) ? timeout : UINT64(~0));
	}
// XXX- overflow
	memcpy(reply->buf, rdyendpts, 4 * nready);
	reply->bufbytes = 4 * nready;
	reply->err = 0;

out:
	free(rdyendpts);
	free(evtcnts);
	free((void *)addrs);
}

static void
smdd_rmnet_open(struct smdd_req *request, struct smdd_reply *reply)
{
	reply->bufbytes = 0;
	reply->err = smd_rmnet_open(request->fd);
}

static void
smdd_dispatch(struct gate_call_data *parm)
{
	struct smdd_req *req = (struct smdd_req *) &parm->param_buf[0];
	struct smdd_reply *reply = (struct smdd_reply *) &parm->param_buf[0];

	static_assert(sizeof(*req) <= sizeof(parm->param_buf));
	static_assert(sizeof(*reply) <= sizeof(parm->param_buf));

	int err = 0;
	try {
		switch (req->op) {
		case tty_open:
			smdd_tty_open(req, reply);
			break;

		case tty_read:
			smdd_tty_read(req, reply);
			break;

		case tty_write:
			smdd_tty_write(req, reply);
			break;

		case tty_close:
			smdd_tty_close(req, reply);
			break;

		case qmi_open:
			smdd_qmi_open(req, reply);
			break;

		case qmi_read:
			smdd_qmi_read(req, reply);
			break;

		case qmi_write:
			smdd_qmi_write(req, reply);
			break;

		case qmi_close:
			smdd_qmi_close(req, reply);
			break;

		case qmi_readwait:
			smdd_qmi_readwait(req, reply);
			break;

		case rpcrouter_create_local_endpoint:
			smdd_rpcrouter_create_local_endpoint(req, reply);
			break;

		case rpcrouter_destroy_local_endpoint:
			smdd_rpcrouter_destroy_local_endpoint(req, reply);
			break;

		case rpc_register_server:
			smdd_rpc_register_server(req, reply);
			break;

		case rpc_unregister_server:
			smdd_rpc_unregister_server(req, reply);
			break;

		case rpc_read:
			smdd_rpc_read(req, reply);
			break;

		case rpc_write:
			smdd_rpc_write(req, reply);
			break;

		case rpc_endpoint_read_select:
			smdd_rpc_endpoint_read_select(req, reply);
			break;

		case get_battery_info:
			smdd_get_battery_info(req, reply);
			break;

		case rmnet_open:
			smdd_rmnet_open(req, reply);
			break;

		default:
			throw error(-E_BAD_OP, "unknown op %d", req->op);
		}
	} catch (error &e) {
		err = e.err();
		printf("%s: %s\n", __func__, e.what());
	}
	reply->err = err;
}

static void __attribute__((noreturn))
smdd_entry(uint64_t x, struct gate_call_data *parm, gatesrv_return *r)
{
	try {
		smdd_dispatch(parm);
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

static int handle_battery_call(struct msm_rpc_server *server,
    struct rpc_request_hdr *req, unsigned len)
{
	return (0);
}

int
main(int argc, char **argv)
try
{
	struct cobj_ref g = gate_create(start_env->root_container,
	    "smdd gate", 0, 0, 0, &smdd_entry, 0);

	fprintf(stderr, "Starting smd core...");
	msm_smd_init();
	smd_tty_init();
	smd_qmi_init();
	smd_rpcrouter_init();
	smd_rpc_servers_init();
	fprintf(stderr, "done.\n");

	static struct msm_rpc_server battery_server;

	battery_server.prog = 0x30100000,
	battery_server.vers = 0,
	battery_server.rpc_call = handle_battery_call;
	msm_rpc_create_server(&battery_server);
	int i = 0;
	do {	
		// keep trying: the rpc server may not be initialized yet
		battery_endpt = msm_rpc_connect(0x30100001, 0, 0);
		if (IS_ERR(battery_endpt)) {
			if (i++ == 100) {
				fprintf(stderr, "msm_rpc_connect failed: %d (%s)\n",
				    PTR_ERR(battery_endpt), e2s(PTR_ERR(battery_endpt)));
				exit(1);
			}
			usleep(100000);
		}
	} while (IS_ERR(battery_endpt));

	thread_halt();
} catch (std::exception &e) {
	printf("smdd: %s\n", e.what());
}
