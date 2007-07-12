#include <linux/socket.h>
#include <linux/if.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/route.h>

#include <linuxsyscall.h>
#include <archcall.h>
#include <os-lib/net.h>

static void
set_ip(struct sockaddr *addr, unsigned int ip)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sin->sin_addr.s_addr = ip;
    sin->sin_family = AF_INET;
}

int
rt_add_gw(int s, unsigned int gw_ip, unsigned int gw_dst, unsigned int gw_nm, 
	  const char *if_alias)
{
    struct rtentry rt;
    long r;
    char if_copy[IFNAMSIZ];
    
    strncpy(if_copy, if_alias, IFNAMSIZ);
    
    /* route to gateway */
    memset(&rt, 0, sizeof(rt));
    set_ip(&rt.rt_dst, gw_dst);
    set_ip(&rt.rt_gateway, 0);
    set_ip(&rt.rt_genmask, gw_nm);
        
    rt.rt_flags = RTF_UP;
    rt.rt_dev = if_copy;
    
    if ((r = linux_ioctl(s, SIOCADDRT, &rt)) < 0) {
	arch_printf("linux_ioctl: error: %ld\n", r);
	return -1;
    }

    /* default route */
    memset(&rt, 0, sizeof(rt));
    set_ip(&rt.rt_dst, 0);
    set_ip(&rt.rt_gateway, gw_ip);
    set_ip(&rt.rt_genmask, 0);
        
    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    rt.rt_dev = if_copy;
    
    if ((r = linux_ioctl(s, SIOCADDRT, &rt)) < 0) {
	arch_printf("linux_ioctl: error: %ld\n", r);
	return -1;
    }
    return 0;
}

int
if_up(int s, const char *if_alias, unsigned int ip)
{
    struct ifreq req;
    struct sockaddr_in *sin;
    int r;
    
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, if_alias, IFNAMSIZ);
    
    if ((r = linux_ioctl(s, SIOCGIFFLAGS, &req)) < 0) {
	arch_printf("linux_ioctl: error: %d\n", r);
	return -1;
    }
    
    req.ifr_flags |= IFF_UP | IFF_BROADCAST;
    
    if ((r = linux_ioctl(s, SIOCSIFFLAGS, &req)) < 0) {
	arch_printf("linux_ioctl: error: %d\n", r);
	return -1;
    }

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, if_alias, IFNAMSIZ);
    sin = (struct sockaddr_in *)&req.ifr_addr;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = ip;
    if ((r = linux_ioctl(s, SIOCSIFADDR, &req)) < 0) {
	arch_printf("linux_ioctl: error: %d\n", r);
	return -1;
    }
    return 0;
}

int
if_print_all(int s)
{
    int i;
    struct ifreq req;
    
    for (i = 1;; i++) {
	memset(&req, 0, sizeof(req));
	req.ifr_ifindex = i;
    
	if (linux_ioctl(s, SIOCGIFNAME, &req) < 0)
	    break;
    
	arch_printf("%s\n", req.ifr_name);
    }
    return 0;
}

int
if_print_up(int s)
{
#define MAX_IFS 16
    struct ifconf ifc;
    struct ifreq *ifrp;
    char buf[sizeof(struct ifreq) * MAX_IFS];
    int remaining, r;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if ((r = linux_ioctl(s, SIOCGIFCONF, &ifc)) < 0) {
	arch_printf("linux_ioctl: error: %d\n", r);
	return -1;
    }

    remaining = ifc.ifc_len;
    ifrp = ifc.ifc_req;

    while (remaining) {
        arch_printf("%s\n", ifrp->ifr_name);
        remaining -= sizeof(*ifrp);
        ifrp = (struct ifreq *) (((char *)ifrp) + sizeof(*ifrp));
    }
    return 0;
}
