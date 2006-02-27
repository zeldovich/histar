extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/fs.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

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

static void
fs_declassify_init(uint64_t net_taint)
{
    struct fs_inode x;
    error_check(fs_namei("/x", &x));

    struct fs_inode mlt;
    error_check(fs_mkmlt(x, "m", &mlt));

    struct fs_inode fsmerge;
    error_check(fs_namei("/bin/fsmerge", &fsmerge));

    label l_th(1);
    l_th.set(net_taint, LB_LEVEL_STAR);

    label l_obj(1);

    const char *argv[3] = { "fsmerge", "/x/m", "/x/m/@mlt" };
    error_check(spawn(start_env->root_container,
		      fsmerge,
		      0, 1, 2,
		      3, &argv[0],
		      l_obj.to_ulabel(), l_th.to_ulabel(),
		      0));
}

int
main(int ac, char **av)
try
{
    uint64_t rc = start_env->root_container;

    int64_t net_grant = sys_handle_create();
    int64_t net_taint = sys_handle_create();

    error_check(net_grant);
    error_check(net_taint);

    netdev_init(rc, net_grant, net_taint);

    struct fs_inode netd_ino;
    error_check(fs_namei("/bin/netd", &netd_ino));

    label gate_ct_label(1);
    gate_ct_label.set(net_grant, 0);
    int64_t gate_ct = sys_container_alloc(rc, gate_ct_label.to_ulabel(),
					  "netd gate");
    error_check(gate_ct);

    label l_th(1);
    l_th.set(net_grant, LB_LEVEL_STAR);
    l_th.set(net_taint, LB_LEVEL_STAR);

    label l_obj(1);
    l_obj.set(net_grant, 0);
    l_obj.set(net_taint, 2);

    char grant_arg[32], taint_arg[32];
    sprintf(grant_arg, "%lu", net_grant);
    sprintf(taint_arg, "%lu", net_taint);

    const char *argv[3];
    argv[0] = "netd";
    argv[1] = grant_arg;
    argv[2] = taint_arg;

    if (netd_mom_debug)
	printf("netd_mom: object label %s, thread label %s\n",
	       l_obj.to_string(), l_th.to_string());

    int64_t ct = spawn(rc, netd_ino, 0, 1, 2, 3, &argv[0],
		       l_obj.to_ulabel(), l_th.to_ulabel(), 0);
    if (ct < 0)
	panic("spawn: %s", e2s(ct));

    fs_declassify_init(net_taint);
} catch (std::exception &e) {
    printf("netd_mom: %s\n", e.what());
}
