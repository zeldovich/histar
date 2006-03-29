extern "C" {
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/netd.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include <jif/tun.h>
}

#include <inc/gatesrv.hh>
#include <inc/netdsrv.hh>
#include <inc/labelutil.hh>

static int netd_debug = 0;

static void
ready_cb(void *arg)
{
    netd_server_enable();
    printf("netd: ready\n");
}

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s tun-device\n", av[0]);
	return -1;
    }

    int64_t grant = sys_handle_create();
    int64_t taint = sys_handle_create();
    assert(grant > 0 && taint > 0);

    if (netd_debug)
	printf("netd: grant handle %ld, taint handle %ld\n",
	       grant, taint);

    struct tun_if tun;
    tun.fd = open(av[1], O_RDWR);
    if (tun.fd < 0) {
	perror("opening tun device");
	exit(-1);
    }

    struct cobj_ref srv;
    try {
	label cntm;
	label clear(2);

	thread_cur_label(&cntm);

	srv = netd_server_init(start_env->shared_container,
			       taint,
			       &cntm, &clear);
    } catch (std::exception &e) {
	panic("%s", e.what());
    }

    netd_lwip_init(&ready_cb, 0, netd_if_tun, &tun,
		   0x0200080a, 0x00ffffff, 0x0100080a);
}
