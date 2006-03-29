extern "C" {
#include <inc/syscall.h>
#include <stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/fs.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>
#include <inc/labelutil.hh>

static int netd_mom_debug = 0;

static void
netdev_init(uint64_t ct, uint64_t net_grant, uint64_t net_taint)
{
    int64_t netdev_id = container_find(ct, kobj_netdev, 0);
    if (netdev_id < 0) {
	label net_label(1);
	net_label.set(net_grant, 0);
	net_label.set(net_taint, 2);
	netdev_id = sys_net_create(ct, net_label.to_ulabel(), "netdev");
	error_check(netdev_id);

	if (netd_mom_debug)
	    printf("netd_mom: netdev %ld grant %ld taint %ld\n",
		   netdev_id, net_grant, net_taint);
    }
}

int
main(int ac, char **av)
try
{
    int64_t net_grant = sys_handle_create();
    int64_t net_taint = sys_handle_create();

    error_check(net_grant);
    error_check(net_taint);

    netdev_init(start_env->shared_container, net_grant, net_taint);

    struct fs_inode netd_ino;
    error_check(fs_namei("/bin/netd", &netd_ino));

    label ds(3);
    ds.set(net_grant, LB_LEVEL_STAR);
    ds.set(net_taint, LB_LEVEL_STAR);

    char grant_arg[32], taint_arg[32];
    sprintf(grant_arg, "%lu", net_grant);
    sprintf(taint_arg, "%lu", net_taint);

    const char *argv[3];
    argv[0] = "netd";
    argv[1] = grant_arg;
    argv[2] = taint_arg;

    if (netd_mom_debug)
	printf("netd_mom: decontaminate-send %s\n", ds.to_string());

    if (netd_mom_debug) {
	label cur;
	thread_cur_label(&cur);
	printf("netd_mom: current label %s\n", cur.to_string());
    }

    spawn(start_env->root_container, netd_ino,
	  0, 1, 2,
	  3, &argv[0],
	  0, 0,
      0, &ds, 0, 0);
} catch (std::exception &e) {
    printf("netd_mom: %s\n", e.what());
}
