extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/fs.h>

#include <stdio.h>
#include <inttypes.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>
#include <inc/labelutil.hh>

static int netd_mom_debug = 0;

static void
netdev_init(uint64_t ct, uint64_t netdev_grant, uint64_t netdev_taint, uint64_t inet_taint)
{
    int64_t netdev_id = container_find(ct, kobj_netdev, 0);
    if (netdev_id < 0) {
	label net_label(1);
	net_label.set(netdev_grant, 0);
	net_label.set(netdev_taint, 3);
	net_label.set(inet_taint, 2);
	netdev_id = sys_net_create(ct, 0, net_label.to_ulabel(), "netdev");
	error_check(netdev_id);

	if (netd_mom_debug)
	    printf("netd_mom: netdev %"PRIu64" grant %"PRIu64" taint %"PRIu64" inet %"PRIu64"\n",
		   netdev_id, netdev_grant, netdev_taint, inet_taint);
    }
}

int
main(int ac, char **av)
try
{
    int64_t netdev_grant = handle_alloc();
    int64_t netdev_taint = handle_alloc();
    int64_t inet_taint = handle_alloc();

    error_check(netdev_grant);
    error_check(netdev_taint);
    error_check(inet_taint);

    netdev_init(start_env->shared_container, netdev_grant, netdev_taint, inet_taint);

    struct fs_inode netd_ino;
    error_check(fs_namei("/bin/netd", &netd_ino));

    label ds(3);
    ds.set(netdev_grant, LB_LEVEL_STAR);
    ds.set(netdev_taint, LB_LEVEL_STAR);
    ds.set(inet_taint, LB_LEVEL_STAR);

    label dr(0);
    dr.set(netdev_grant, 3);
    dr.set(netdev_taint, 3);
    dr.set(inet_taint, 3);

    char grant_arg[32], taint_arg[32], inet_arg[32];
    sprintf(grant_arg, "%"PRIu64, netdev_grant);
    sprintf(taint_arg, "%"PRIu64, netdev_taint);
    sprintf(inet_arg,  "%"PRIu64, inet_taint);

    const char *argv[] = { "netd", grant_arg, taint_arg, inet_arg };

    if (netd_mom_debug)
	printf("netd_mom: decontaminate-send %s\n", ds.to_string());

    if (netd_mom_debug) {
	label cur;
	thread_cur_label(&cur);
	printf("netd_mom: current label %s\n", cur.to_string());
    }

    label co(0);
    co.set(inet_taint, 2);

    spawn(start_env->root_container, netd_ino,
	  0, 1, 2,
	  4, &argv[0],
	  0, 0,
	  0, &ds, 0, &dr, &co, SPAWN_NO_AUTOGRANT);
} catch (std::exception &e) {
    printf("netd_mom: %s\n", e.what());
}
