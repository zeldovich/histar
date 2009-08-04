#include "rpc/rpc.h"
#include "../msm_rpcrouter.h"
#include "../smd_rpcrouter.h"
#include "debug.h"

extern "C" {
#include <sys/types.h>   
#include <sys/stat.h>     
#include <fcntl.h>        
#include <unistd.h>      
#include <stdio.h>
#include <errno.h>

#include "../support/smddgate.h"

#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
}

#define DUMP_DATA 0

/* XXXX- this is so un-threadsafe! */

// list of file descriptors
static void *fdlist[256];

int r_dup(int fd)
{
	//cprintf("%s: on fd %d\n", __func__, fd);

	if (fd < 0 || fd >= 256 || fdlist[fd] == NULL)
		return -1;

	int i;
	for (i = 0; i < 256; i++) {
		if (fdlist[i] == NULL)
			break;
	}
	if (i == 256)
		return -1;

	fdlist[i] = fdlist[fd];

	//cprintf("%s: duplicated fd %d to %d\n", __func__, fd, i);
	return i;
}

int r_open(const char *router)
{
	//cprintf("%s: on %s\n", __func__, router);

	char *progstr = strrchr(router, '/');
	if (progstr == NULL)
		return -E_INVAL;

	progstr++;

	char *versstr = strchr(progstr, ':');
	if (versstr == NULL)
		return -E_INVAL;

	*versstr = '\0';
	versstr++;

	uint32_t prog = strtoul(progstr, NULL, 16);
	uint32_t vers = strtoul(versstr, NULL, 16); 

	int i;
	for (i = 0; i < 256; i++) {
		if (fdlist[i] == NULL)
			break;
	}
	if (i == 256)
		return -E_NO_MEM;

	fdlist[i] = smddgate_rpcrouter_create_local_endpoint(1, prog, vers);
	if(fdlist[i] == NULL) {
		E("error opening %s: %s\n", router, strerror(errno));
		return -E_NOT_FOUND;
	}

	//cprintf("%s: opened fd %d at 0x%08x:0x%08x\n", __func__, i, prog, vers);
	return i;
}

void r_close(int fd)
{
	//cprintf("%s: on fd %d\n", __func__, fd);

	if (fd < 0 || fd >= 256 || fdlist[fd] == NULL) {
		E("error: %s\n", strerror(errno));
	} else {
		/* make sure we aren't dup'd */
		int preserve = 0;
		for (int i = 0; i < 256; i++) {
			if (i != fd && fdlist[i] == fdlist[fd])
				preserve = 1;
		}
		if (!preserve)
			msm_rpcrouter_destroy_local_endpoint(fdlist[fd]);
		else
			//cprintf("%s: preserving endpoint (fd must have been dup'd)\n", __func__);
		fdlist[fd] = NULL;
	}
}

int r_read(int fd, char *buf, uint32 size)
{
	struct rr_fragment *frag, *next;
	int rc;

	//cprintf("%s: on fd %d, size %u\n", __func__, fd, size);

	if (fd < 0 || fd >= 256 || fdlist[fd] == NULL)
		return -E_INVAL;

	rc = smddgate_rpc_read(fdlist[fd], buf, size);
	if (rc < 0) {
		E("error reading RPC packet: %d (%s)\n", errno, strerror(errno));
		return rc;
	}

#if DUMP_DATA
	{
		int len = rc / 4;
		uint32_t *data = (uint32_t *)buf;
		fprintf(stdout, "RPC in  %02d:", rc);
		while (len--)
			fprintf(stdout, " %08x", *data++);
		fprintf(stdout, "\n");
	}
#endif
	return rc;
}

int r_write (int fd, const char *buf, uint32 size)
{
	//cprintf("%s: on fd %d\n", __func__, fd);

	if (fd < 0 || fd >= 256 || fdlist[fd] == NULL)
		return -E_INVAL;

	if (size > RPCROUTER_MSGSIZE_MAX)
		return -E_INVAL;

	// msm_rpc_write will modify the buf, but r_write takes const char * 
	char *wbuf = (char *)malloc(size); 
	memcpy(wbuf, buf, size);

	int rc = smddgate_rpc_write(fdlist[fd], (void *)wbuf, size);
	free(wbuf);
	if (rc < 0) {
		E("error writing RPC packet: %d (%s)\n", errno, strerror(errno));
		return rc;
	}

#if DUMP_DATA
	{
		int len = rc / 4;
		uint32_t *data = (uint32_t *)buf;
		fprintf(stdout, "RPC out %02d:", rc);
		while (len--)
			fprintf(stdout, " %08x", *data++);
		fprintf(stdout, "\n");
	}
#endif

	return rc;
}

int r_control(int fd, const uint32 cmd, void *arg)
{
	int n;
	void *ept;
	struct rpcrouter_ioctl_server_args *server_args;

	cprintf("%s: on fd %d, cmd 0x%08x\n", __func__, fd, cmd);

	if (fd < 0 || fd >= 256 || fdlist[fd] == NULL)
		return -E_INVAL;

	ept = fdlist[fd];

	// XXXXXXX- fix the IOCTLs here. We need to use the linux values that
	// libgps.so expects.

	switch (cmd) {
	case RPC_ROUTER_IOCTL_GET_VERSION:
		n = RPC_ROUTER_VERSION_V1;
		*(unsigned int *)arg = n;
		return 0;	

	case RPC_ROUTER_IOCTL_GET_MTU:
		/* the pacmark word reduces the actual payload
		 * possible per message
		 */
		n = RPCROUTER_MSGSIZE_MAX - sizeof(uint32_t);
		*(unsigned int *)arg = n;
		return 0;

	case RPC_ROUTER_IOCTL_REGISTER_SERVER:
		server_args = (struct rpcrouter_ioctl_server_args *)arg;
		smddgate_rpc_register_server(ept,
					server_args->prog,
					server_args->vers);
		return 0;

	case RPC_ROUTER_IOCTL_UNREGISTER_SERVER:
		server_args = (struct rpcrouter_ioctl_server_args *)arg;
		smddgate_rpc_unregister_server(ept,
					  server_args->prog,
					  server_args->vers);
		return 0;

	default:
		cprintf("%s: unhandled ioctl 0x%08x\n", __func__, cmd);
		return -E_INVAL;
	}
}

int r_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
    struct timeval *timeout)
{
	//cprintf("%s: on nfds %d, timeout @ %p (%u sec, %u usec)\n", __func__, nfds, timeout, (timeout != NULL) ? (unsigned)timeout->tv_sec : 0, (timeout != NULL) ? (unsigned)timeout->tv_usec : 0);

	assert(readfds != NULL);
	assert(writefds == NULL);
	assert(errorfds == NULL);

	uint64_t *evtcnts = (uint64_t *)malloc(nfds * sizeof(evtcnts[0]));
	if (evtcnts == NULL)
		return -E_NO_MEM;

	volatile uint64_t **addrs  = (volatile uint64_t **)malloc(nfds * sizeof(addrs[0]));
	if (addrs == NULL) { 
		free(evtcnts);
		return -E_NO_MEM;
	}

	uint64_t now_nsec = sys_clock_nsec();

	int nready;
	while (1) {
		int idx = 0;

		nready = 0;
		for (int i = 0; i < nfds && i < 256; i++) { 
			if (FD_ISSET(i, readfds)) {
				struct msm_rpc_endpoint *ept = fdlist[i];
				if (ept == NULL) {
					//cprintf("%s: warning - invalid fd %d passed in\n", __func__, i);
					FD_CLR(i, readfds);
				} else {
					pthread_mutex_lock(&ept->read_q_lock);
					if (!TAILQ_EMPTY(&ept->read_q)) {
						nready++;
					} else {
						FD_CLR(i, readfds);
						pthread_mutex_lock(&ept->waitq_mutex);
						evtcnts[idx] =  ept->waitq_evtcnt;
						addrs[idx++] = &ept->waitq_evtcnt;
						pthread_mutex_unlock(&ept->waitq_mutex);
					}
					pthread_mutex_unlock(&ept->read_q_lock);
				}
			}
		}

		if (nready)
			break;

		uint64_t wait_nsec = 0;
		if (timeout != NULL) {
			wait_nsec  = (uint64_t)timeout->tv_sec * 1000000000ULL;
			wait_nsec += (uint64_t)timeout->tv_usec * 1000ULL;
		}

		// if no fds to check, optionally sleep and return 0
		if (idx == 0) {
			if (timeout != NULL)
				usleep(wait_nsec / 1000);
			return 0;
		}

		if (timeout != NULL)
			wait_nsec += now_nsec;
		else
			wait_nsec = UINT64(~0);

		sys_sync_wait_multi(addrs, evtcnts, NULL, idx, wait_nsec);
	}

	free(evtcnts);
	free((void *)addrs);

	return nready;
}
