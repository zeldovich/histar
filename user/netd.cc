extern "C" {
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/netd.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
}

#include <inc/gatesrv.hh>
#include <inc/netdsrv.hh>
#include <inc/labelutil.hh>

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

    uint64_t grant, taint, inet_taint;
    int r = strtou64(av[1], 0, 10, &grant);
    if (r < 0)
	panic("parsing grant handle %s: %s", av[1], e2s(r));

    r = strtou64(av[2], 0, 10, &taint);
    if (r < 0)
	panic("parsing taint handle %s: %s", av[2], e2s(r));

    r = strtou64(av[3], 0, 10, &inet_taint);
    if (r < 0)
	panic("parsing inet taint handle %s: %s", av[3], e2s(r));

    if (netd_debug)
	printf("netd: grant handle %"PRIu64", taint handle %"PRIu64"\n",
	       grant, taint);

    try {
	label cntm;
	label clear;

	thread_cur_label(&cntm);
	thread_cur_clearance(&clear);
	if (netd_do_taint)
	    cntm.set(inet_taint, 2);

	netd_server_init(start_env->shared_container,
			 inet_taint, &cntm, &clear);

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
