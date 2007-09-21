extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

int
main(int ac, char **av)
try
{
    if (ac != 2) {
	printf("Usage: %s vpn-server\n", av[0]);
	return -1;
    }
    char *vpn_host = av[1];

    // Create a new mount table
    cobj_ref old_mtab = start_env->fs_mtab_seg;

    int64_t new_mtab_id =
	sys_segment_copy(old_mtab, start_env->shared_container,
			 0, "vpn mtab");
    error_check(new_mtab_id);
    cobj_ref new_mtab = COBJ(start_env->shared_container, new_mtab_id);
    start_env->fs_mtab_seg = new_mtab;

    fs_inode shared_ct;
    fs_get_root(start_env->shared_container, &shared_ct);
    fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "vpnstart", shared_ct);

    label tun_label(1);
    fs_inode tun_ct;
    fs_mkdir(shared_ct, "tun", &tun_ct, tun_label.to_ulabel());

    fs_inode tun_buf;
    fs_create(tun_ct, "buf", &tun_buf, 0);

    if (fork() == 0) {
	// Start OpenVPN
	const char *argv[] = {
	    "openvpn",
	    "--dev", "tun",
	    "--dev-node", "/vpnstart/tun/buf@tun-a",
	    "--proto", "tcp-client",
	    "--remote", vpn_host,
	    0,
	    };
	if (execv("/bin/openvpn", (char * const *) &argv[0]) < 0)
	    perror("execv openvpn");
	thread_halt();
    }

    if (fork() == 0) {
	fs_inode netd_vpn_ct;
	fs_get_root(start_env->shared_container, &netd_vpn_ct);

	start_env->fs_mtab_seg = old_mtab;
	fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "netd-vpn", netd_vpn_ct);
	start_env->fs_mtab_seg = new_mtab;

	// Start netd_vpn
	const char *argv[] = { "netd_vpn", "/vpnstart/tun/buf@tun-b", 0 };
	if (execv("/bin/netd_vpn", (char * const *) &argv[0]) < 0)
	    perror("execv netd_vpn");
	thread_halt();
    }

    printf("vpn_start: running with VPN server %s\n", vpn_host);
    thread_halt();
} catch (std::exception &e) {
    printf("vpn_start: %s\n", e.what());
}
