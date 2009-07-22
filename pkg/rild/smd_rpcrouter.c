/* arch/arm/mach-msm/smd_rpcrouter.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

/* TODO: fragmentation for large writes */
/* TODO: handle cases where smd_write() will tempfail due to full fifo */
/* TODO: thread priority? schedule a work to bump it? */
/* TODO: maybe make server_list_lock a mutex */
/* TODO: pool fragments to avoid kmalloc/free churn */

extern "C" {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include <sys/mman.h>

#include <inc/atomic.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/queue.h>
#include <inc/syscall.h>
}

#include "msm_smd.h"
#include "smd_private.h"
#include "smd_rpcrouter.h"
#include "support/misc.h"

#define TRACE_R2R_MSG 0
#define TRACE_R2R_RAW 0
#define TRACE_RPC_MSG 0

#define MSM_RPCROUTER_DEBUG 1
#define MSM_RPCROUTER_DEBUG_PKT 0
#define MSM_RPCROUTER_R2R_DEBUG 0
#define DUMP_ALL_RECEIVED_HEADERS 0

#define DIAG(x...) cprintf("[RR] ERROR " x)

#if MSM_RPCROUTER_DEBUG
#define D(x...) cprintf(x)
#else
#define D(x...) do {} while (0)
#endif

#if TRACE_R2R_MSG
#define RR(x...) cprintf("[RR] "x)
#else
#define RR(x...) do {} while (0)
#endif

#if TRACE_RPC_MSG
#define IO(x...) cprintf("[RPC] "x)
#else
#define IO(x...) do {} while (0)
#endif

static TAILQ_HEAD(, msm_rpc_endpoint) local_endpoints = TAILQ_HEAD_INITIALIZER(head);
static TAILQ_HEAD(, rr_remote_endpoint) remote_endpoints = TAILQ_HEAD_INITIALIZER(head);

static TAILQ_HEAD(, rr_server) server_list = TAILQ_HEAD_INITIALIZER(head);

static smd_channel_t *smd_channel;
static int initialized;
static pthread_mutex_t	newserver_waitq_mutex;
static pthread_cond_t	newserver_waitq_cond;
static pthread_mutex_t	smd_waitq_mutex;
static pthread_cond_t	smd_waitq_cond;

static pthread_mutex_t local_endpoints_lock;
static pthread_mutex_t remote_endpoints_lock;
static pthread_mutex_t server_list_lock;
static pthread_mutex_t smd_lock;

#if 0
static struct wake_lock rpcrouter_wake_lock;
#endif
static int rpcrouter_need_len;

static jos_atomic_t next_xid = { 1 };
static uint8_t next_pacmarkid;

static void do_read_data(void);
static void do_create_pdevs(void);
static void do_create_rpcrouter_pdev(void);

#define RR_STATE_IDLE    0
#define RR_STATE_HEADER  1
#define RR_STATE_BODY    2
#define RR_STATE_ERROR   3

struct rr_context {
	struct rr_packet *pkt;
	uint8_t *ptr;
	uint32_t state; /* current assembly state */
	uint32_t count; /* bytes needed in this state */
};

struct rr_context the_rr_context;

static uint32_t
atomic_add_return(uint32_t incr, jos_atomic_t *atom)
{
	uint32_t res, cur;

	do {
		cur = atom->counter;
		res = jos_atomic_compare_exchange(atom, cur, cur + incr);
	} while (res != cur);

	return (cur + incr);
} 

static void
msleep(unsigned msecs)
{
	usleep(msecs * 1000);
}

static int rpcrouter_send_control_msg(union rr_control_msg *msg)
{
	struct rr_header hdr;
	int need;

	if (!(msg->cmd == RPCROUTER_CTRL_CMD_HELLO) && !initialized) {
		cprintf("rpcrouter_send_control_msg(): Warning, "
		       "router not initialized\n");
		return -E_INVAL;
	}

	hdr.version = RPCROUTER_VERSION;
	hdr.type = msg->cmd;
	hdr.src_pid = RPCROUTER_PID_LOCAL;
	hdr.src_cid = RPCROUTER_ROUTER_ADDRESS;
	hdr.confirm_rx = 0;
	hdr.size = sizeof(*msg);
	hdr.dst_pid = 0;
	hdr.dst_cid = RPCROUTER_ROUTER_ADDRESS;

	/* TODO: what if channel is full? */

	need = sizeof(hdr) + hdr.size;
	pthread_mutex_lock(&smd_lock);
	while (smd_write_avail(smd_channel) < need) {
		pthread_mutex_unlock(&smd_lock);
		msleep(250);
		pthread_mutex_lock(&smd_lock);
	}
	smd_write(smd_channel, &hdr, sizeof(hdr));
	smd_write(smd_channel, msg, hdr.size);
	pthread_mutex_unlock(&smd_lock);
	return 0;
}

static struct rr_server *rpcrouter_create_server(uint32_t pid,
							uint32_t cid,
							uint32_t prog,
							uint32_t ver)
{
	struct rr_server *server;
	int rc;

	server = (struct rr_server *)malloc(sizeof(struct rr_server));
	if (!server)
		return (struct rr_server *)ERR_PTR(-E_NO_MEM);

	memset(server, 0, sizeof(struct rr_server));
	server->pid = pid;
	server->cid = cid;
	server->prog = prog;
	server->vers = ver;

	pthread_mutex_lock(&server_list_lock);
	TAILQ_INSERT_TAIL(&server_list, server, list);
	pthread_mutex_unlock(&server_list_lock);

	if (pid == RPCROUTER_PID_REMOTE) {
#ifdef notyet
		rc = msm_rpcrouter_create_server_cdev(server);
		if (rc < 0)
#else
		rc = -E_INVAL;
#endif
			goto out_fail;
	}
	return server;
out_fail:
	pthread_mutex_lock(&server_list_lock);
	TAILQ_REMOVE(&server_list, server, list);
	pthread_mutex_unlock(&server_list_lock);
	free(server);
	return (struct rr_server *)ERR_PTR(rc);
}

static void rpcrouter_destroy_server(struct rr_server *server)
{

	pthread_mutex_lock(&server_list_lock);
	TAILQ_REMOVE(&server_list, server, list);
	pthread_mutex_unlock(&server_list_lock);
#ifdef notyet
	device_destroy(msm_rpcrouter_class, server->device_number);
#endif
	free(server);
}

static struct rr_server *rpcrouter_lookup_server(uint32_t prog,
							uint32_t ver)
{
	struct rr_server *server;

	pthread_mutex_lock(&server_list_lock);
	TAILQ_FOREACH(server, &server_list, list) {
		if (server->prog == prog
		 && server->vers == ver) {
			pthread_mutex_unlock(&server_list_lock);
			return server;
		}
	}
	pthread_mutex_unlock(&server_list_lock);
	return NULL;
}

#ifdef notyet
static struct rr_server *rpcrouter_lookup_server_by_dev(dev_t dev)
{
	struct rr_server *server;

	pthread_mutex_lock(&server_list_lock);
	TAILQ_FOREACH(server, &server_list, list) {
		if (server->device_number == dev) {
			pthread_mutex_unlock(&server_list_lock);
			return server;
		}
	}
	pthread_mutex_unlock(&server_list_lock);
	return NULL;
}
#endif

struct msm_rpc_endpoint *msm_rpcrouter_create_local_endpoint(dev_t dev)
{
	struct msm_rpc_endpoint *ept;

	ept = (struct msm_rpc_endpoint *)malloc(sizeof(struct msm_rpc_endpoint));
	if (!ept)
		return NULL;
	memset(ept, 0, sizeof(struct msm_rpc_endpoint));

	/* mark no reply outstanding */
	ept->reply_pid = 0xffffffff;

	ept->cid = (uint32_t) ept;
	ept->pid = RPCROUTER_PID_LOCAL;
	ept->dev = dev;

#ifdef notyet
	if ((dev != msm_rpcrouter_devno) && (dev != MKDEV(0, 0))) {
		struct rr_server *srv;
		/*
		 * This is a userspace client which opened
		 * a program/ver devicenode. Bind the client
		 * to that destination
		 */
		srv = rpcrouter_lookup_server_by_dev(dev);
		/* TODO: bug? really? */
		BUG_ON(!srv);

		ept->dst_pid = srv->pid;
		ept->dst_cid = srv->cid;
		ept->dst_prog = cpu_to_be32(srv->prog);
		ept->dst_vers = cpu_to_be32(srv->vers);
	} else {
		/* mark not connected */
		ept->dst_pid = 0xffffffff;
	}
#else
		ept->dst_pid = 0xffffffff;
#endif

	pthread_mutex_init(&ept->waitq_mutex, NULL);
	pthread_cond_init(&ept->waitq_cond, NULL);
	TAILQ_INIT(&ept->read_q);
	pthread_mutex_init(&ept->read_q_lock, NULL);
#if 0
	wake_lock_init(&ept->read_q_wake_lock, WAKE_LOCK_SUSPEND, "rpc_read");
#endif
	TAILQ_INIT(&ept->incomplete);

	pthread_mutex_lock(&local_endpoints_lock);
	TAILQ_INSERT_TAIL(&local_endpoints, ept, list);
	pthread_mutex_unlock(&local_endpoints_lock);
	return ept;
}

int msm_rpcrouter_destroy_local_endpoint(struct msm_rpc_endpoint *ept)
{
	int rc;
	union rr_control_msg msg;

	msg.cmd = RPCROUTER_CTRL_CMD_REMOVE_CLIENT;
	msg.cli.pid = ept->pid;
	msg.cli.cid = ept->cid;

	RR("x REMOVE_CLIENT id=%d:%08x\n", ept->pid, ept->cid);
	rc = rpcrouter_send_control_msg(&msg);
	if (rc < 0)
		return rc;

#if 0
	wake_lock_destroy(&ept->read_q_wake_lock);
#endif
	TAILQ_REMOVE(&local_endpoints, ept, list);
	free(ept);
	return 0;
}

static int rpcrouter_create_remote_endpoint(uint32_t cid)
{
	struct rr_remote_endpoint *new_c;

	new_c = (struct rr_remote_endpoint *)malloc(sizeof(struct rr_remote_endpoint));
	if (!new_c)
		return -E_NO_MEM;
	memset(new_c, 0, sizeof(struct rr_remote_endpoint));

	new_c->cid = cid;
	new_c->pid = RPCROUTER_PID_REMOTE;
	pthread_mutex_init(&new_c->quota_lock, NULL);

	pthread_mutex_lock(&remote_endpoints_lock);
	TAILQ_INSERT_HEAD(&remote_endpoints, new_c, list);
	pthread_mutex_unlock(&remote_endpoints_lock);
	return 0;
}

static struct msm_rpc_endpoint *rpcrouter_lookup_local_endpoint(uint32_t cid)
{
	struct msm_rpc_endpoint *ept;

	pthread_mutex_lock(&local_endpoints_lock);
	TAILQ_FOREACH(ept, &local_endpoints, list) {
		if (ept->cid == cid) {
			pthread_mutex_unlock(&local_endpoints_lock);
			return ept;
		}
	}
	pthread_mutex_unlock(&local_endpoints_lock);
	return NULL;
}

static struct rr_remote_endpoint *rpcrouter_lookup_remote_endpoint(uint32_t cid)
{
	struct rr_remote_endpoint *ept;

	pthread_mutex_lock(&remote_endpoints_lock);
	TAILQ_FOREACH(ept, &remote_endpoints, list) {
		if (ept->cid == cid) {
			pthread_mutex_unlock(&remote_endpoints_lock);
			return ept;
		}
	}
	pthread_mutex_unlock(&remote_endpoints_lock);
	return NULL;
}

static int process_control_msg(union rr_control_msg *msg, int len)
{
	union rr_control_msg ctl;
	struct rr_server *server;
	struct rr_remote_endpoint *r_ept;
	int rc = 0;

	if (len != sizeof(*msg)) {
		cprintf("rpcrouter: r2r msg size %d != %d\n",
		       len, sizeof(*msg));
		return -E_INVAL;
	}

	switch (msg->cmd) {
	case RPCROUTER_CTRL_CMD_HELLO:
		RR("o HELLO\n");

		RR("x HELLO\n");
		memset(&ctl, 0, sizeof(ctl));
		ctl.cmd = RPCROUTER_CTRL_CMD_HELLO;
		rpcrouter_send_control_msg(&ctl);

		initialized = 1;

		/* Send list of servers one at a time */
		ctl.cmd = RPCROUTER_CTRL_CMD_NEW_SERVER;

		/* TODO: long time to hold a spinlock... */
		pthread_mutex_lock(&server_list_lock);
		TAILQ_FOREACH(server, &server_list, list) {
			ctl.srv.pid = server->pid;
			ctl.srv.cid = server->cid;
			ctl.srv.prog = server->prog;
			ctl.srv.vers = server->vers;

			RR("x NEW_SERVER id=%d:%08x prog=%08x:%d\n",
			   server->pid, server->cid,
			   server->prog, server->vers);

			rpcrouter_send_control_msg(&ctl);
		}
		pthread_mutex_unlock(&server_list_lock);

		do_create_rpcrouter_pdev();
		break;

	case RPCROUTER_CTRL_CMD_RESUME_TX:
		RR("o RESUME_TX id=%d:%08x\n", msg->cli.pid, msg->cli.cid);

		r_ept = rpcrouter_lookup_remote_endpoint(msg->cli.cid);
		if (!r_ept) {
			cprintf("rpcrouter: Unable to resume client\n");
			break;
		}
		pthread_mutex_lock(&r_ept->quota_lock);
		r_ept->tx_quota_cntr = 0;
		pthread_mutex_unlock(&r_ept->quota_lock);

		pthread_mutex_lock(&r_ept->quota_waitq_mutex);
		pthread_cond_signal(&r_ept->quota_waitq_cond);
		pthread_mutex_unlock(&r_ept->quota_waitq_mutex);
		break;

	case RPCROUTER_CTRL_CMD_NEW_SERVER:
		RR("o NEW_SERVER id=%d:%08x prog=%08x:%d\n",
		   msg->srv.pid, msg->srv.cid, msg->srv.prog, msg->srv.vers);

		server = rpcrouter_lookup_server(msg->srv.prog, msg->srv.vers);

		if (!server) {
			server = rpcrouter_create_server(
				msg->srv.pid, msg->srv.cid,
				msg->srv.prog, msg->srv.vers);
			if (!server)
				return -E_NO_MEM;
			/*
			 * XXX: Verify that its okay to add the
			 * client to our remote client list
			 * if we get a NEW_SERVER notification
			 */
			if (!rpcrouter_lookup_remote_endpoint(msg->srv.cid)) {
				rc = rpcrouter_create_remote_endpoint(
					msg->srv.cid);
				if (rc < 0)
					cprintf("rpcrouter:Client create"
						"error (%d)\n", rc);
			}
			do_create_pdevs();

			pthread_mutex_lock(&newserver_waitq_mutex);
			pthread_cond_signal(&newserver_waitq_cond);
			pthread_mutex_unlock(&newserver_waitq_mutex);
		} else {
			if ((server->pid == msg->srv.pid) &&
			    (server->cid == msg->srv.cid)) {
				cprintf("rpcrouter: Duplicate svr\n");
			} else {
				server->pid = msg->srv.pid;
				server->cid = msg->srv.cid;
			}
		}
		break;

	case RPCROUTER_CTRL_CMD_REMOVE_SERVER:
		RR("o REMOVE_SERVER prog=%08x:%d\n",
		   msg->srv.prog, msg->srv.vers);
		server = rpcrouter_lookup_server(msg->srv.prog, msg->srv.vers);
		if (server)
			rpcrouter_destroy_server(server);
		break;

	case RPCROUTER_CTRL_CMD_REMOVE_CLIENT:
		RR("o REMOVE_CLIENT id=%d:%08x\n", msg->cli.pid, msg->cli.cid);
		if (msg->cli.pid != RPCROUTER_PID_REMOTE) {
			cprintf("rpcrouter: Denying remote removal of "
			       "local client\n");
			break;
		}
		r_ept = rpcrouter_lookup_remote_endpoint(msg->cli.cid);
		if (r_ept) {
			pthread_mutex_lock(&remote_endpoints_lock);
			TAILQ_REMOVE(&remote_endpoints, r_ept, list);
			pthread_mutex_unlock(&remote_endpoints_lock);
			free(r_ept);
		}

		/* Notify local clients of this event */
		cprintf("rpcrouter: LOCAL NOTIFICATION NOT IMP\n");
		rc = -E_INVAL;

		break;
	default:
		RR("o UNKNOWN(%08x)\n", msg->cmd);
		rc = -E_INVAL;
	}

	return rc;
}

static void do_create_rpcrouter_pdev()
{
#ifdef notyet
	platform_device_register(&rpcrouter_pdev);
#endif
}

static void do_create_pdevs()
{
	struct rr_server *server;

	/* TODO: race if destroyed while being registered */
	pthread_mutex_lock(&server_list_lock);
	TAILQ_FOREACH(server, &server_list, list) {
		if (server->pid == RPCROUTER_PID_REMOTE) {
			if (server->pdev_name[0] == 0) {
				pthread_mutex_unlock(&server_list_lock);
#ifdef notyet
				msm_rpcrouter_create_server_pdev(server);
#endif
				do_create_pdevs();
				return;
			}
		}
	}
	pthread_mutex_unlock(&server_list_lock);
}

static void rpcrouter_smdnotify(void *_dev, unsigned event)
{
	if (event != SMD_EVENT_DATA)
		return;

#if 0
	if (smd_read_avail(smd_channel) >= rpcrouter_need_len)
		wake_lock(&rpcrouter_wake_lock);
#endif

	pthread_mutex_lock(&smd_waitq_mutex);
	pthread_cond_signal(&smd_waitq_cond);
	pthread_mutex_unlock(&smd_waitq_mutex);
}

static void *rr_malloc(unsigned sz)
{
	void *ptr = malloc(sz);
	if (ptr)
		return ptr;

	cprintf("rpcrouter: malloc of %d failed, retrying...\n", sz);
	do {
		ptr = malloc(sz);
	} while (!ptr);

	return ptr;
}

/* TODO: deal with channel teardown / restore */
static int rr_read(void *data, int len)
{
	int rc;
//	cprintf("rr_read() %d\n", len);
	for(;;) {
		pthread_mutex_lock(&smd_lock);
		if (smd_read_avail(smd_channel) >= len) {
			rc = smd_read(smd_channel, data, len);
			pthread_mutex_unlock(&smd_lock);
			if (rc == len)
				return 0;
			else
				return -E_IO;
		}
		rpcrouter_need_len = len;
#if 0
		wake_unlock(&rpcrouter_wake_lock);
#endif
		pthread_mutex_unlock(&smd_lock);

//		cprintf("rr_read: waiting (%d)\n", len);

		pthread_mutex_lock(&smd_waitq_mutex);
		if (!(smd_read_avail(smd_channel) >= len))
			pthread_cond_wait(&smd_waitq_cond, &smd_waitq_mutex);
		pthread_mutex_unlock(&smd_waitq_mutex);
	}
	return 0;
}

static uint32_t r2r_buf[RPCROUTER_MSGSIZE_MAX];

static void do_read_data()
{
	struct rr_header hdr;
	struct rr_packet *pkt;
	struct rr_fragment *frag;
	struct msm_rpc_endpoint *ept;
	uint32_t pm, mid;

	if (rr_read(&hdr, sizeof(hdr)))
		goto fail_io;

#if TRACE_R2R_RAW
	RR("- ver=%d type=%d src=%d:%08x crx=%d siz=%d dst=%d:%08x\n",
	   hdr.version, hdr.type, hdr.src_pid, hdr.src_cid,
	   hdr.confirm_rx, hdr.size, hdr.dst_pid, hdr.dst_cid);
#endif

	if (hdr.version != RPCROUTER_VERSION) {
		DIAG("version %d != %d\n", hdr.version, RPCROUTER_VERSION);
		goto fail_data;
	}
	if (hdr.size > RPCROUTER_MSGSIZE_MAX) {
		DIAG("msg size %d > max %d\n", hdr.size, RPCROUTER_MSGSIZE_MAX);
		goto fail_data;
	}

	if (hdr.dst_cid == RPCROUTER_ROUTER_ADDRESS) {
		if (rr_read(r2r_buf, hdr.size))
			goto fail_io;
		process_control_msg((rr_control_msg *) r2r_buf, hdr.size);
		goto done;
	}

	if (hdr.size < sizeof(pm)) {
		DIAG("runt packet (no pacmark)\n");
		goto fail_data;
	}
	if (rr_read(&pm, sizeof(pm)))
		goto fail_io;

	hdr.size -= sizeof(pm);

	frag = (struct rr_fragment *)rr_malloc(hdr.size + sizeof(*frag));
	frag->next = NULL;
	frag->length = hdr.size;
	if (rr_read(frag->data, hdr.size))
		goto fail_io;

	ept = rpcrouter_lookup_local_endpoint(hdr.dst_cid);
	if (!ept) {
		DIAG("no local ept for cid %08x\n", hdr.dst_cid);
		free(frag);
		goto done;
	}
	
	/* See if there is already a partial packet that matches our mid
	 * and if so, append this fragment to that packet.
	 */
	mid = PACMARK_MID(pm);

	struct rr_packet *next;
	for (pkt = TAILQ_FIRST(&ept->incomplete); pkt != NULL; pkt = next) {
		next = TAILQ_NEXT(pkt, list);

		if (pkt->mid == mid) {
			pkt->last->next = frag;
			pkt->last = frag;
			pkt->length += frag->length;
			if (PACMARK_LAST(pm)) {
				TAILQ_REMOVE(&ept->incomplete, pkt, list);
				goto packet_complete;
			}
			goto done;
		}
	}

	/* This mid is new -- create a packet for it, and put it on
	 * the incomplete list if this fragment is not a last fragment,
	 * otherwise put it on the read queue.
	 */
	pkt = (struct rr_packet *)rr_malloc(sizeof(struct rr_packet));
	pkt->first = frag;
	pkt->last = frag;
	memcpy(&pkt->hdr, &hdr, sizeof(hdr));
	pkt->mid = mid;
	pkt->length = frag->length;
	if (!PACMARK_LAST(pm)) {
		TAILQ_INSERT_TAIL(&ept->incomplete, pkt, list);
		goto done;
	}

packet_complete:
	pthread_mutex_lock(&ept->read_q_lock);
#if 0
	wake_lock(&ept->read_q_wake_lock);
#endif
	TAILQ_INSERT_TAIL(&ept->read_q, pkt, list);

	pthread_mutex_lock(&ept->waitq_mutex);
	pthread_cond_signal(&ept->waitq_cond);
	pthread_mutex_unlock(&ept->waitq_mutex);

	pthread_mutex_unlock(&ept->read_q_lock);
done:
	if (hdr.confirm_rx) {
		union rr_control_msg msg;

		msg.cmd = RPCROUTER_CTRL_CMD_RESUME_TX;
		msg.cli.pid = hdr.dst_pid;
		msg.cli.cid = hdr.dst_cid;
	
		RR("x RESUME_TX id=%d:%08x\n", msg.cli.pid, msg.cli.cid);
		rpcrouter_send_control_msg(&msg);
	}

	do_read_data();
	return;

fail_io:
fail_data:
	cprintf("rpc_router has died\n");
#if 0
	wake_unlock(&rpcrouter_wake_lock);
#endif
}

void msm_rpc_setup_req(struct rpc_request_hdr *hdr, uint32_t prog,
		       uint32_t vers, uint32_t proc)
{
	memset(hdr, 0, sizeof(struct rpc_request_hdr));
	hdr->xid = cpu_to_be32(atomic_add_return(1, &next_xid));
	hdr->rpc_vers = cpu_to_be32(2);
	hdr->prog = cpu_to_be32(prog);
	hdr->vers = cpu_to_be32(vers);
	hdr->procedure = cpu_to_be32(proc);
}

struct msm_rpc_endpoint *msm_rpc_open(void)
{
	struct msm_rpc_endpoint *ept;

	ept = msm_rpcrouter_create_local_endpoint(MKDEV(0, 0));
	if (ept == NULL)
		return (struct msm_rpc_endpoint *)ERR_PTR(-E_NO_MEM);

	return ept;
}

int msm_rpc_close(struct msm_rpc_endpoint *ept)
{
	return msm_rpcrouter_destroy_local_endpoint(ept);
}

int msm_rpc_write(struct msm_rpc_endpoint *ept, void *buffer, int count)
{
	struct rr_header hdr;
	uint32_t pacmark;
	struct rpc_request_hdr *rq = (struct rpc_request_hdr *)buffer;
	struct rr_remote_endpoint *r_ept;
	int needed;

	/* TODO: fragmentation for large outbound packets */
	if (count > (int)(RPCROUTER_MSGSIZE_MAX - sizeof(uint32_t)) || !count)
		return -E_INVAL;

	/* snoop the RPC packet and enforce permissions */

	/* has to have at least the xid and type fields */
	if (count < (int)(sizeof(uint32_t) * 2)) {
		cprintf("rr_write: rejecting runt packet\n");
		return -E_INVAL;
	}

	if (rq->type == 0) {
		/* RPC CALL */
		if (count < (int)(sizeof(uint32_t) * 6)) {
			cprintf("rr_write: rejecting runt call packet\n");
			return -E_INVAL;
		}
		if (ept->dst_pid == 0xffffffff) {
			cprintf("rr_write: not connected\n");
			return -E_INVAL;
		}
		if ((ept->dst_prog != rq->prog) ||
		    (ept->dst_vers != rq->vers)) {
			cprintf("rr_write: cannot write to %08x:%d "
			       "(bound to %08x:%d)\n",
			       be32_to_cpu(rq->prog), be32_to_cpu(rq->vers),
			       be32_to_cpu(ept->dst_prog),
			       be32_to_cpu(ept->dst_vers));
			return -E_INVAL;
		}
		hdr.dst_pid = ept->dst_pid;
		hdr.dst_cid = ept->dst_cid;
		IO("CALL to %08x:%d @ %d:%08x (%d bytes)\n",
		   be32_to_cpu(rq->prog), be32_to_cpu(rq->vers),
		   ept->dst_pid, ept->dst_cid, count);
	} else {
		/* RPC REPLY */
		/* TODO: locking */
		if (ept->reply_pid == 0xffffffff) {
			cprintf("rr_write: rejecting unexpected reply\n");
			return -E_INVAL;
		}
		if (ept->reply_xid != rq->xid) {
			cprintf("rr_write: rejecting packet w/ bad xid\n");
			return -E_INVAL;
		}

		hdr.dst_pid = ept->reply_pid;
		hdr.dst_cid = ept->reply_cid;

		/* consume this reply */
		ept->reply_pid = 0xffffffff;

		IO("REPLY to xid=%d @ %d:%08x (%d bytes)\n",
		   be32_to_cpu(rq->xid), hdr.dst_pid, hdr.dst_cid, count);
	}

	r_ept = rpcrouter_lookup_remote_endpoint(hdr.dst_cid);

	if (!r_ept) {
		cprintf("msm_rpc_write(): No route to ept "
			"[PID %x CID %x]\n", hdr.dst_pid, hdr.dst_cid);
		return -E_INVAL;
	}

	/* Create routing header */
	hdr.type = RPCROUTER_CTRL_CMD_DATA;
	hdr.version = RPCROUTER_VERSION;
	hdr.src_pid = ept->pid;
	hdr.src_cid = ept->cid;
	hdr.confirm_rx = 0;
	hdr.size = count + sizeof(uint32_t);

	for (;;) {
		pthread_mutex_lock(&r_ept->quota_lock);
		if (r_ept->tx_quota_cntr < RPCROUTER_DEFAULT_RX_QUOTA)
			break;
#if 0
		if (signal_pending(current) && 
		    (!(ept->flags & MSM_RPC_UNINTERRUPTIBLE)))
			break;
#endif
		pthread_mutex_unlock(&r_ept->quota_lock);
		sys_self_yield();
	}

#if 0
	if (signal_pending(current) &&
	    (!(ept->flags & MSM_RPC_UNINTERRUPTIBLE))) {
		pthread_mutex_unlock(&r_ept->quota_lock);
		return -E_RESTARTSYS;
	}
#endif

	r_ept->tx_quota_cntr++;
	if (r_ept->tx_quota_cntr == RPCROUTER_DEFAULT_RX_QUOTA)
		hdr.confirm_rx = 1;

	/* bump pacmark while interrupts disabled to avoid race
	 * probably should be atomic op instead
	 */
	pacmark = PACMARK(count, ++next_pacmarkid, 1);

	pthread_mutex_unlock(&r_ept->quota_lock);

	pthread_mutex_lock(&smd_lock);

	needed = sizeof(hdr) + hdr.size;
	while (smd_write_avail(smd_channel) < needed) {
		pthread_mutex_unlock(&smd_lock);
		msleep(250);
		pthread_mutex_lock(&smd_lock);
	}

	/* TODO: deal with full fifo */
	smd_write(smd_channel, &hdr, sizeof(hdr));
	smd_write(smd_channel, &pacmark, sizeof(pacmark));
	smd_write(smd_channel, buffer, count);

	pthread_mutex_unlock(&smd_lock);

	return count;
}

/*
 * NOTE: It is the responsibility of the caller to free buffer
 */
int msm_rpc_read(struct msm_rpc_endpoint *ept, void **buffer,
		 unsigned user_len, long timeout)
{
	struct rr_fragment *frag, *next;
	char *buf;
	int rc;

	rc = __msm_rpc_read(ept, &frag, user_len, timeout);
	if (rc <= 0)
		return rc;

	/* single-fragment messages conveniently can be
	 * returned as-is (the buffer is at the front)
	 */
	if (frag->next == 0) {
		*buffer = (void*) frag;
		return rc;
	}

	/* multi-fragment messages, we have to do it the
	 * hard way, which is rather disgusting right now
	 */
	buf = (char *)rr_malloc(rc);
	*buffer = buf;

	while (frag != NULL) {
		memcpy(buf, frag->data, frag->length);
		next = frag->next;
		buf += frag->length;
		free(frag);
		frag = next;
	}

	return rc;
}

int msm_rpc_call(struct msm_rpc_endpoint *ept, uint32_t proc,
		 void *_request, int request_size,
		 long timeout)
{
	return msm_rpc_call_reply(ept, proc,
				  (rr_control_msg *)_request, request_size,
				  NULL, 0, timeout);
}

int msm_rpc_call_reply(struct msm_rpc_endpoint *ept, uint32_t proc,
		       void *_request, int request_size,
		       void *_reply, int reply_size,
		       long timeout)
{
	struct rpc_request_hdr *req = (struct rpc_request_hdr *)_request;
	struct rpc_reply_hdr *reply;
	int rc;

	if (request_size < (int)sizeof(*req))
		return -E_INVAL;

	if (ept->dst_pid == 0xffffffff)
		return -E_INVAL;

	memset(req, 0, sizeof(*req));
	req->xid = cpu_to_be32(atomic_add_return(1, &next_xid));
	req->rpc_vers = cpu_to_be32(2);
	req->prog = ept->dst_prog;
	req->vers = ept->dst_vers;
	req->procedure = cpu_to_be32(proc);

	rc = msm_rpc_write(ept, req, request_size);
	if (rc < 0)
		return rc;

	for (;;) {
		rc = msm_rpc_read(ept, (void **)(void *) &reply, -1, timeout);
		if (rc < 0)
			return rc;
		if (rc < (int)(3 * sizeof(uint32_t))) {
			rc = -E_IO;
			break;
		}
		/* we should not get CALL packets -- ignore them */
		if (reply->type == 0) {
			free(reply);
			continue;
		}
		/* If an earlier call timed out, we could get the (no
		 * longer wanted) reply for it.  Ignore replies that
		 * we don't expect
		 */
		if (reply->xid != req->xid) {
			free(reply);
			continue;
		}
		if (reply->reply_stat != 0) {
			rc = -E_LABEL;
			break;
		}
		if (reply->data.acc_hdr.accept_stat != 0) {
			rc = -E_INVAL;
			break;
		}
		if (_reply == NULL) {
			rc = 0;
			break;
		}
		if (rc > reply_size) {
			rc = -E_NO_MEM;
		} else {
			memcpy(_reply, reply, rc);
		}
		break;
	}
	free(reply);
	return rc;
}


static inline int ept_packet_available(struct msm_rpc_endpoint *ept)
{
	int ret;
	pthread_mutex_lock(&ept->read_q_lock);
	ret = !TAILQ_EMPTY(&ept->read_q);
	pthread_mutex_unlock(&ept->read_q_lock);
	return ret;
}

int __msm_rpc_read(struct msm_rpc_endpoint *ept,
		   struct rr_fragment **frag_ret,
		   unsigned len, long msecs)
{
	struct rr_packet *pkt;
	struct rpc_request_hdr *rq;
	int rc = 0;

	IO("READ on ept %p\n", ept);

	if (ept->flags & MSM_RPC_UNINTERRUPTIBLE) {
		if (msecs < 0) {
			pthread_mutex_lock(&ept->waitq_mutex);
			if (!ept_packet_available(ept))
				pthread_cond_wait(&ept->waitq_cond, &ept->waitq_mutex);
			pthread_mutex_unlock(&ept->waitq_mutex);
		} else {
			pthread_mutex_lock(&ept->waitq_mutex);
			if (!ept_packet_available(ept))
				rc = pthread_cond_timeout_np(&ept->waitq_cond, &ept->waitq_mutex, msecs);
			pthread_mutex_unlock(&ept->waitq_mutex);

			if (rc == ETIMEDOUT)
				return -ETIMEDOUT;
		}
	} else {
		if (msecs < 0) {
			pthread_mutex_lock(&ept->waitq_mutex);
			if (!ept_packet_available(ept))
				pthread_cond_wait(&ept->waitq_cond, &ept->waitq_mutex);
			pthread_mutex_unlock(&ept->waitq_mutex);
		} else {
			pthread_mutex_lock(&ept->waitq_mutex);
			if (!ept_packet_available(ept))
				rc = pthread_cond_timeout_np(&ept->waitq_cond, &ept->waitq_mutex, msecs);
			pthread_mutex_unlock(&ept->waitq_mutex);

			if (rc != 0)
				return -ETIMEDOUT;
		}
	}

	pthread_mutex_lock(&ept->read_q_lock);
	if (TAILQ_EMPTY(&ept->read_q)) {
		pthread_mutex_unlock(&ept->read_q_lock);
		return -E_AGAIN;
	}
	pkt = (struct rr_packet *)TAILQ_FIRST(&ept->read_q);
	if (pkt->length > len) {
		pthread_mutex_unlock(&ept->read_q_lock);
		return -E_INVAL;
	}
	TAILQ_REMOVE(&ept->read_q, pkt, list);
#if 0
	if (TAILQ_EMPTY(&ept->read_q))
		wake_unlock(&ept->read_q_wake_lock);
#endif
	pthread_mutex_unlock(&ept->read_q_lock);

	rc = pkt->length;

	*frag_ret = pkt->first;
	rq = (rpc_request_hdr *) pkt->first->data;
	if ((rc >= (int)(sizeof(uint32_t) * 3)) && (rq->type == 0)) {
		/* RPC CALL */
		if (ept->reply_pid != 0xffffffff) {
			cprintf("rr_read: lost previous reply xid...\n");
		}
		/* TODO: locking? */
		ept->reply_pid = pkt->hdr.src_pid;
		ept->reply_cid = pkt->hdr.src_cid;
		ept->reply_xid = rq->xid;
	}

	free(pkt);

	IO("READ on ept %p (%d bytes)\n", ept, rc);
	return rc;
}

struct msm_rpc_endpoint *msm_rpc_connect(uint32_t prog, uint32_t vers, unsigned flags)
{
	struct msm_rpc_endpoint *ept;
	struct rr_server *server;

	server = rpcrouter_lookup_server(prog, vers);
	if (!server)
		return (struct msm_rpc_endpoint *)ERR_PTR(-E_NOT_FOUND);

	ept = msm_rpc_open();
	if (IS_ERR(ept))
		return ept;
	
	ept->flags = flags;
	ept->dst_pid = server->pid;
	ept->dst_cid = server->cid;
	ept->dst_prog = cpu_to_be32(prog);
	ept->dst_vers = cpu_to_be32(vers);

	return ept;
}

/* TODO: permission check? */
int msm_rpc_register_server(struct msm_rpc_endpoint *ept,
			    uint32_t prog, uint32_t vers)
{
	int rc;
	union rr_control_msg msg;
	struct rr_server *server;

	server = rpcrouter_create_server(ept->pid, ept->cid,
					 prog, vers);
	if (!server)
		return -E_INVAL;

	msg.srv.cmd = RPCROUTER_CTRL_CMD_NEW_SERVER;
	msg.srv.pid = ept->pid;
	msg.srv.cid = ept->cid;
	msg.srv.prog = prog;
	msg.srv.vers = vers;

	RR("x NEW_SERVER id=%d:%08x prog=%08x:%d\n",
	   ept->pid, ept->cid, prog, vers);

	rc = rpcrouter_send_control_msg(&msg);
	if (rc < 0)
		return rc;

	return 0;
}

/* TODO: permission check -- disallow unreg of somebody else's server */
int msm_rpc_unregister_server(struct msm_rpc_endpoint *ept,
			      uint32_t prog, uint32_t vers)
{
	struct rr_server *server;
	server = rpcrouter_lookup_server(prog, vers);

	if (!server)
		return -E_NOT_FOUND;
	rpcrouter_destroy_server(server);
	return 0;
}

static int msm_rpcrouter_probe()
{
	int rc;

	/* Initialize what we need to start processing */
#if 0
	wake_lock_init(&rpcrouter_wake_lock, WAKE_LOCK_SUSPEND, "SMD_RPCCALL");
#endif

	pthread_mutex_init(&newserver_waitq_mutex, NULL);
	pthread_cond_init(&newserver_waitq_cond, NULL);
	pthread_mutex_init(&smd_waitq_mutex, NULL);
	pthread_cond_init(&smd_waitq_cond, NULL);

	pthread_mutex_init(&local_endpoints_lock, NULL);
	pthread_mutex_init(&remote_endpoints_lock, NULL);
	pthread_mutex_init(&server_list_lock, NULL);
	pthread_mutex_init(&smd_lock, NULL);

#ifdef notyet
	rc = msm_rpcrouter_init_devices();
	if (rc < 0)
		return rc;
#endif
	/* Open up SMD channel 2 */
	initialized = 0;
	rc = smd_open("SMD_RPCCALL", &smd_channel, NULL, rpcrouter_smdnotify);
	if (rc < 0) {
#ifdef notyet
		msm_rpcrouter_exit_devices();
#endif
		return rc;
	}

	do_read_data();
	return 0;
}

int
smd_rpcrouter_init()
{
	return msm_rpcrouter_probe();
}
