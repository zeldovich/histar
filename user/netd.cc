extern "C" {
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <stdio.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <string.h>
#include <inc/netd.h>
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
    if (ac != 3) {
	printf("Usage: %s grant-handle taint-handle\n", av[0]);
	return -1;
    }

    uint64_t grant, taint;
    int r = strtou64(av[1], 0, 10, &grant);
    if (r < 0)
	panic("parsing grant handle %s: %s", av[1], e2s(r));

    r = strtou64(av[2], 0, 10, &taint);
    if (r < 0)
	panic("parsing taint handle %s: %s", av[2], e2s(r));

    if (netd_debug)
	printf("netd: grant handle %ld, taint handle %ld\n",
	       grant, taint);

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

    netd_lwip_init(&ready_cb, 0, netd_if_jif, 0, 0, 0, 0);
}
