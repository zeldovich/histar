#include "rpc/rpc.h"
#include "../smdd/msm_rpcrouter.h"
#include "../smdd/smd_rpcrouter.h"
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

#if 0
#define DPRINTF(_x)	cprintf _x
#else
#define DPRINTF(_x)
#endif

/* XXXX- this is so un-threadsafe! */

// list of file descriptors
static void *fdlist[256];

int r_dup(int fd)
{
	DPRINTF(("%s: on fd %d\n", __func__, fd));

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
	DPRINTF(("%s: on %s\n", __func__, router));

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
	DPRINTF(("%s: on fd %d\n", __func__, fd));

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
			smddgate_rpcrouter_destroy_local_endpoint(fdlist[fd]);
		else
			//cprintf("%s: preserving endpoint (fd must have been dup'd)\n", __func__);
		fdlist[fd] = NULL;
	}
}

int r_read(int fd, char *buf, uint32 size)
{
	int rc;

	DPRINTF(("%s: on fd %d, size %u\n", __func__, fd, size));

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

int r_write(int fd, const char *buf, uint32 size)
{
	DPRINTF(("%s: on fd %d\n", __func__, fd));

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
	DPRINTF(("%s: on nfds %d, timeout @ %p (%u sec, %u usec)\n", __func__, nfds, timeout, (timeout != NULL) ? (unsigned)timeout->tv_sec : 0, (timeout != NULL) ? (unsigned)timeout->tv_usec : 0));

	assert(readfds != NULL);
	assert(writefds == NULL);
	assert(errorfds == NULL);

	uint64_t now_nsec = sys_clock_nsec();
	uint64_t wait_nsec = now_nsec;
	if (timeout != NULL) {
		wait_nsec += (uint64_t)timeout->tv_sec * 1000000000ULL;
		wait_nsec += (uint64_t)timeout->tv_usec * 1000ULL;
	}

	void **endpts = (void **)malloc(nfds * sizeof(endpts[0]));
	if (endpts == NULL)
		return -E_NO_MEM;

	int idx = 0;
	for (int i = 0; i < nfds && i < 256; i++) { 
		if (FD_ISSET(i, readfds)) {
			void *ept = fdlist[i];
			if (ept == NULL) {
				cprintf("%s: warning - invalid fd %d passed in\n", __func__, i);
			} else {
				endpts[idx++] = ept;
			}
		}
	}

	// if no fds to check, optionally sleep and return 0
	if (idx == 0) {
		if (timeout != NULL)
			usleep((wait_nsec - now_nsec) / 1000);
		return 0;
	}

	int r = smddgate_rpc_endpoint_read_select(endpts, idx, (timeout != NULL) ? wait_nsec : 0);

	//ick
	for (int i = 0; i < nfds; i++) {
		if (FD_ISSET(i, readfds)) {
			void *ept = fdlist[i];
			int found = 0;
			for (int j = 0; j < r; j++) {
				if (ept == endpts[j]) {
					found = 1;
					break;
				}
			} 
			if (!found)
				FD_CLR(i, readfds);
		}
	}

	free(endpts);
	return r;
}