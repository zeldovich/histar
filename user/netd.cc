extern "C" {
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/assert.h>
#include <netd/netdlwip.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <arch/sys_arch.h>
#include <netif/etharp.h>

#include <inttypes.h>
#include <pthread.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <netd/netdsrv.hh>

#ifdef JOS_ARCH_arm
#include <pkg/htcdream/smdd/msm_rpcrouter2.h>
#include <inc/smdd.h>
#include <pkg/htcdream/support/smddgate.h>
#endif

static int netd_debug = 0;
enum { netd_do_taint = 0 };

static uint32_t netd_ip, netd_mask, netd_gw;

#ifdef JOS_ARCH_arm
static void
htc_poll_address(uint32_t *ip, uint32_t *mask, uint32_t *gw, int zero_ok)
{
    struct htc_netconfig netcfg; 
    int report = 0;

    while (1) {
	smddgate_rmnet_config(0, &netcfg);
	*ip   = htonl(netcfg.ip);
	*mask = htonl(netcfg.mask);
	*gw   = htonl(netcfg.gw);
	if (*ip != 0)
		break;
	if (zero_ok)
		break;

	if (!report) {
		printf("netd waiting on smdd for IP address...\n");
		report++;
	}
	sleep(5);
    }

    printf("netd using HTC ip 0x%08x, mask 0x%08x, gw 0x%08x\n", *ip, *mask, *gw);

    FILE *fp = fopen("/netd/resolv.conf", "w");
    fprintf(fp, "nameserver %d.%d.%d.%d\n", (netcfg.dns1 >> 24) & 0xff,
                                            (netcfg.dns1 >> 16) & 0xff,
                                            (netcfg.dns1 >>  8) & 0xff,
                                            (netcfg.dns1 >>  0) & 0xff);
    fprintf(fp, "nameserver %d.%d.%d.%d\n", (netcfg.dns2 >> 24) & 0xff,
                                            (netcfg.dns2 >> 16) & 0xff,
                                            (netcfg.dns2 >>  8) & 0xff,
                                            (netcfg.dns2 >>  0) & 0xff);
    fclose(fp);
}

static void *
htc_poll_thread(void *arg)
{
    struct netif *nif = (struct netif *)arg;

    while (1) {
        uint32_t ip, mask, gw;

	htc_poll_address(&ip, &mask, &gw, 1);

	struct ip_addr ipaddr, netmask, gateway;
	ipaddr.addr  = ip;
	netmask.addr = mask;
	gateway.addr = gw;

        if (ip != netd_ip) {
		if (ip == 0) {
			printf("netd: ip address == 0: network down\n");
			lwip_core_lock();
			netif_set_down(nif);
			lwip_core_unlock();
		} else {
			printf("netd: ip address != 0: network up!\n");
			lwip_core_lock();
			netif_set_down(nif);
			netif_set_addr(nif, &ipaddr, &netmask, &gateway);
			netif_set_up(nif);
			lwip_core_unlock();
		}
	}

	netd_ip = ip;
	netd_mask = mask;
	netd_gw = gw;

	// gcc, shut up about the noreturn attribute candidacy shit
	if (ip == 0 && mask != 0 && gw != 0) break;

	sleep(2);
    }

    return NULL;
}
#endif

static void
ready_cb(void *arg)
{
    struct netif *nif = (struct netif *)arg;

    netd_server_enable();
    printf("netd: ready\n");

#ifdef JOS_ARCH_arm
    // network will go up and down a lot, that's the nature of the beast
    // so create a thread to check for this
    pthread_t pid;
    pthread_create(&pid, NULL, htc_poll_thread, nif);
#endif
}

int
main(int ac, char **av)
{
    if (ac != 4) {
	printf("Usage: %s grant-handle taint-handle inet-taint\n", av[0]);
	return -1;
    }

#ifdef JOS_ARCH_arm
    // XXX- nasty hack for HTC Dream
    smddgate_init();
    htc_poll_address(&netd_ip, &netd_mask, &netd_gw, 0);
#endif

    struct cobj_ref netdev;

    try {
	error_check(fs_clone_mtab(start_env->shared_container));

	fs_inode self;
	fs_get_root(start_env->shared_container, &self);
	fs_unmount(start_env->fs_mtab_seg, start_env->fs_root, "netd");
	error_check(fs_mount(start_env->fs_mtab_seg, start_env->fs_root,
			     "netd", self));

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

	    n = strlen("netdev=");
	    if (!strncmp(av[i], "netdev=", n)) {
		char *ctid = av[i] + n;
		char *dot = strchr(ctid, '.');
		if (dot) {
		    error_check(strtou64(ctid, 0, 10, &netdev.container));
		    error_check(strtou64(dot+1, 0, 10, &netdev.object));
		    continue;
		}
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

    netd_lwip_init(&ready_cb, 0, netd_if_jif, &netdev, netd_ip, netd_mask, netd_gw);
}
