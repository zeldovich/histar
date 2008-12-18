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
netdev_init(uint64_t ct, uint64_t netdev_grant, uint64_t netdev_taint, uint64_t inet_taint, cobj_ref *ndev)
{
    int64_t netdev_id = container_find(ct, kobj_device, 0);
    if (netdev_id < 0) {
	label net_label;
	net_label.add(netdev_grant);
	net_label.add(netdev_taint);
#if 0	/* XXX */
	net_label.add(inet_taint);
#endif
	netdev_id = sys_device_create(ct, 0, net_label.to_ulabel(), "jif0",
                                      device_net);
	error_check(netdev_id);

	if (netd_mom_debug)
	    printf("netd_mom: netdev %"PRIu64" grant %"PRIu64" taint %"PRIu64" inet %"PRIu64"\n",
		   netdev_id, netdev_grant, netdev_taint, inet_taint);

	*ndev = COBJ(ct, netdev_id);
    }
}

static struct child_process
start_lwip(label *taint, label *owner, label *clear, const char *netdev,
	   const char *grant_arg, const char *taint_arg, const char *inet_arg)
{
    struct fs_inode netd_ino;
    error_check(fs_namei("/bin/netd", &netd_ino));
    const char *argv[] = { "netd", netdev, grant_arg, taint_arg, inet_arg };
    return
	spawn(start_env->process_pool, netd_ino,
		0, 1, 2,
		4, argv,
		0, 0,
		taint, owner, clear, SPAWN_NO_AUTOGRANT);
}

static struct child_process
start_linux(label *taint, label *owner, label *clear, const char *netdev,
	   const char *grant_arg, const char *taint_arg, const char *inet_arg)
{
    struct fs_inode netd_ino;
    const char *vmlinux_pn = "/bin/vmlinux";
    const char *argv[] = { vmlinux_pn,
			   "ip=dhcp", "initrd=/bin/initrd", "loglevel=4",
			   netdev, grant_arg, taint_arg, inet_arg };
    int argc = sizeof(argv) / sizeof(char *);
    error_check(fs_namei(vmlinux_pn, &netd_ino));

    return
	spawn(start_env->process_pool, netd_ino,
		0, 1, 2,
		argc, argv,
		0, 0,
		taint, owner, clear, SPAWN_NO_AUTOGRANT);
}

static void
mount_netd(struct child_process cp)
{
    struct fs_inode ino;
    fs_get_root(cp.container, &ino);
    fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "netd", ino);
}

int
main(int ac, char **av)
try
{
    cobj_ref ndev_obj = COBJ(0, 0);
    int64_t netdev_grant = category_alloc(0);
    int64_t netdev_taint = category_alloc(1);
    int64_t inet_taint = category_alloc(1);

    error_check(netdev_grant);
    error_check(netdev_taint);
    error_check(inet_taint);

    netdev_init(start_env->shared_container,
		netdev_grant, netdev_taint, inet_taint, &ndev_obj);

    label owner;
    owner.add(netdev_grant);
    owner.add(netdev_taint);
    owner.add(inet_taint);

    char grant_arg[64], taint_arg[64], inet_arg[64], netdev[64];
    sprintf(grant_arg, "netdev_grant=%"PRIu64, netdev_grant);
    sprintf(taint_arg, "netdev_taint=%"PRIu64, netdev_taint);
    sprintf(inet_arg,  "inet_taint=%"PRIu64, inet_taint);
    sprintf(netdev, "netdev=%"PRIu64".%"PRIu64,
	    ndev_obj.container, ndev_obj.object);

    if (netd_mom_debug)
	printf("netd_mom: ownership %s\n", owner.to_string());

    if (netd_mom_debug) {
	label cur;
	thread_cur_label(&cur);
	printf("netd_mom: current label %s\n", cur.to_string());
    }

    label taint;
#if 0	/* XXX */
    taint.add(inet_taint);
#endif

    try {
	mount_netd(start_lwip(&taint, &owner, 0, netdev,
			      grant_arg, taint_arg, inet_arg));
    } catch (std::exception &e) {
	if (netd_mom_debug)
	    printf("netd_mom: unable to start lwip: %s\n", e.what());
	mount_netd(start_linux(&taint, &owner, 0, netdev,
			       grant_arg, taint_arg, inet_arg));
    }
} catch (std::exception &e) {
    printf("netd_mom: %s\n", e.what());
}
