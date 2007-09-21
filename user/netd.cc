extern "C" {
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/netd.h>
#include <netd/netdlwip.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <netd/netdsrv.hh>

static int netd_debug = 0;
enum { netd_do_taint = 0 };

static void
ready_cb(void *arg)
{
    netd_server_enable();
    printf("netd: ready\n");
}

int
main(int ac, char **av)
{
    if (ac != 4) {
	printf("Usage: %s grant-handle taint-handle inet-taint\n", av[0]);
	return -1;
    }

    try {
	uint64_t grant, taint, inet_taint;

	for (int i = 1; i < 4; i++) {
	    int n = strlen("netdev_grant=");
	    if (!strncmp(av[i], "netdev_grant=", n)) {
		error_check(strtou64(av[i] + n, 0, 10, &grant));
		continue;
	    }
	    
	    n = strlen("netdev_taint=");
	    if (!strncmp(av[i], "netdev_taint=", n)) {
		error_check(strtou64(av[i] + n, 0, 10, &taint));
		continue;
	    }
	    
	    n = strlen("inet_taint=");
	    if (!strncmp(av[i], "inet_taint=", n)) {
		error_check(strtou64(av[i] + n, 0, 10, &inet_taint));
		continue;
	    }
	    
	    printf("Unknown argument: %s\n", av[i]);
	    return -1;
	}
	
	if (netd_debug)
	    printf("netd: grant handle %"PRIu64", taint handle %"PRIu64"\n",
		   grant, taint);
	
	label cntm;
	label clear;

	thread_cur_label(&cntm);
	thread_cur_clearance(&clear);
	if (netd_do_taint)
	    cntm.set(inet_taint, 2);

	netd_server_init(start_env->shared_container,
			 inet_taint, &cntm, &clear, netd_lwip_dispatch);

	// Disable signals -- the signal gate has { inet_taint:* }
	int64_t sig_gt = container_find(start_env->shared_container, kobj_gate, "signal");
	error_check(sig_gt);
	error_check(sys_obj_unref(COBJ(start_env->shared_container, sig_gt)));

	thread_set_label(&cntm);
    } catch (std::exception &e) {
	panic("%s", e.what());
    }

    netd_lwip_init(&ready_cb, 0, netd_if_jif, 0, 0, 0, 0);
}
