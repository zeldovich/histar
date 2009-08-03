/* arch/arm/mach-msm/rpc_servers.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Iliyan Malchev <ibm@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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

#include <inc/atomic.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/queue.h>
#include <inc/syscall.h>
}

#include "msm_smd.h"
#include "smd_private.h"
#include "smd_rpcrouter.h"
#include "../support/misc.h"

static struct msm_rpc_endpoint *endpoint;

#define FLAG_REGISTERED 0x0001

static LIST_HEAD(, msm_rpc_server) rpc_server_list = LIST_HEAD_INITIALIZER(head);
static pthread_mutex_t rpc_server_list_lock;
static int rpc_servers_active;
//static struct wake_lock rpc_servers_wake_lock;

static void rpc_server_register(struct msm_rpc_server *server)
{
	int rc;
	rc = msm_rpc_register_server(endpoint, server->prog, server->vers);
	if (rc < 0)
		cprintf("[rpcserver] error registering %p @ %08x:%d\n",
		       server, server->prog, server->vers);
}

static struct msm_rpc_server *rpc_server_find(uint32_t prog, uint32_t vers)
{
	struct msm_rpc_server *server;

	pthread_mutex_lock(&rpc_server_list_lock);
	LIST_FOREACH(server, &rpc_server_list, list) {
		if ((server->prog == prog) && (server->vers == vers)) {
			pthread_mutex_unlock(&rpc_server_list_lock);
			return server;
		}
	}
	pthread_mutex_unlock(&rpc_server_list_lock);
	return NULL;
}

static void rpc_server_register_all(void)
{
	struct msm_rpc_server *server;

	pthread_mutex_lock(&rpc_server_list_lock);
	LIST_FOREACH(server, &rpc_server_list, list) {
		if (!(server->flags & FLAG_REGISTERED)) {
			rpc_server_register(server);
			server->flags |= FLAG_REGISTERED;
		}
	}
	pthread_mutex_unlock(&rpc_server_list_lock);
}

int msm_rpc_create_server(struct msm_rpc_server *server)
{
	/* make sure we're in a sane state first */
	server->flags = 0;
	memset(&server->list, 0, sizeof(server->list));

	pthread_mutex_lock(&rpc_server_list_lock);
	LIST_INSERT_HEAD(&rpc_server_list, server, list);
	if (rpc_servers_active) {
		rpc_server_register(server);
		server->flags |= FLAG_REGISTERED;
	}
	pthread_mutex_unlock(&rpc_server_list_lock);

	return 0;
}

static int rpc_send_accepted_void_reply(struct msm_rpc_endpoint *client,
					uint32_t xid, uint32_t accept_status)
{
	int rc = 0;
	uint8_t reply_buf[sizeof(struct rpc_reply_hdr)];
	struct rpc_reply_hdr *reply = (struct rpc_reply_hdr *)reply_buf;

	reply->xid = cpu_to_be32(xid);
	reply->type = cpu_to_be32(1); /* reply */
	reply->reply_stat = cpu_to_be32(RPCMSG_REPLYSTAT_ACCEPTED);

	reply->data.acc_hdr.accept_stat = cpu_to_be32(accept_status);
	reply->data.acc_hdr.verf_flavor = 0;
	reply->data.acc_hdr.verf_length = 0;

	rc = msm_rpc_write(endpoint, reply_buf, sizeof(reply_buf));
	if (rc < 0)
		cprintf("%s: could not write response: %d\n",
		       __FUNCTION__, rc);

	return rc;
}

static void *rpc_servers_thread(void *arg)
{
	void *buffer;
	struct rpc_request_hdr *req;
	struct msm_rpc_server *server;
	int rc;

	for (;;) {
//		wake_unlock(&rpc_servers_wake_lock);
		pthread_mutex_lock(&endpoint->waitq_mutex);
		if (TAILQ_EMPTY(&endpoint->read_q))
			pthread_cond_wait(&endpoint->waitq_cond, &endpoint->waitq_mutex);
		pthread_mutex_unlock(&endpoint->waitq_mutex);
//		wake_lock(&rpc_servers_wake_lock);
		rc = msm_rpc_read(endpoint, &buffer, -1, -1);
		if (rc < 0) {
			cprintf("%s: could not read: %d\n",
			       __FUNCTION__, rc);
			break;
		}
		req = (struct rpc_request_hdr *)buffer;

		req->type = be32_to_cpu(req->type);
		req->xid = be32_to_cpu(req->xid);
		req->rpc_vers = be32_to_cpu(req->rpc_vers);
		req->prog = be32_to_cpu(req->prog);
		req->vers = be32_to_cpu(req->vers);
		req->procedure = be32_to_cpu(req->procedure);

		server = rpc_server_find(req->prog, req->vers);

		if (req->rpc_vers != 2)
			continue;
		if (req->type != 0)
			continue;
		if (!server) {
			rpc_send_accepted_void_reply(
				endpoint, req->xid,
				RPC_ACCEPTSTAT_PROG_UNAVAIL);
			continue;
		}

		rc = server->rpc_call(server, req, rc);

		switch (rc) {
		case 0:
			rpc_send_accepted_void_reply(
				endpoint, req->xid,
				RPC_ACCEPTSTAT_SUCCESS);
			break;
		default:
			rpc_send_accepted_void_reply(
				endpoint, req->xid,
				RPC_ACCEPTSTAT_PROG_UNAVAIL);
			break;
		}

		free(buffer);
	}

	pthread_exit(NULL);
}

static int rpcservers_probe(void)
{
	endpoint = msm_rpc_open();
	if (IS_ERR(endpoint))
		return PTR_ERR(endpoint);

	/* we're online -- register any servers installed beforehand */
	rpc_servers_active = 1;
	rpc_server_register_all();

	/* start the thread */
	pthread_t tid;
	pthread_create(&tid, NULL, rpc_servers_thread, NULL);

	return 0;
}

int smd_rpc_servers_init (void)
{
//	wake_lock_init(&rpc_servers_wake_lock, WAKE_LOCK_SUSPEND, "rpc_server");
	return (rpcservers_probe()); 
}
