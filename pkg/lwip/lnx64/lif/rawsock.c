#include <lif/rawsock.h>

#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

#include <linux/if_packet.h>
#include <linux/if.h>
#include <sys/ioctl.h>


#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

// XXX
static struct ifreq ifr_cleanup;
static int sock_cleanup;

static void
raw_cleanup(void)
{
    if (ioctl(sock_cleanup, SIOCSIFFLAGS, &ifr_cleanup) < 0)
	perror("linux_cleanup: ioctl");
}

static int
raw_enable_permisc(int s, const char *iface_alias)
{
    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface_alias, IFNAMSIZ);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
	printf("ioctl error: %s", strerror(errno));
	return -1;
    }

    // why doesn't the 'new' way work?
    struct packet_mreq pr;
    memset(&pr, 0, sizeof(pr));
    pr.mr_ifindex = ifr.ifr_ifindex;
    pr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(s, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		   &pr, sizeof(pr) < 0)) {
	
	// try old fashion way
	struct ifreq ifr2;
	strncpy(ifr2.ifr_name, iface_alias, IFNAMSIZ);
	if (ioctl(s, SIOCGIFFLAGS, &ifr2) < 0) {
	    printf("ioctl error: %s", strerror(errno));
	    return -1;
	}
	memcpy(&ifr_cleanup, &ifr2, sizeof(ifr_cleanup));
	sock_cleanup = s;
	ifr2.ifr_flags |= IFF_PROMISC;
	if (ioctl(s, SIOCSIFFLAGS, &ifr2) < 0) {
	    printf("ioctl error: %s", strerror(errno));
	    return -1;
	}
	atexit(raw_cleanup);
    }

    return 0;
}

int
raw_socket(const char *iface_alias, char *mac_addr)
{
    int s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (s < 0)
	return s;
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface_alias, IFNAMSIZ);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
	return -1;
    
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(s, (struct sockaddr *) &sll, sizeof(sll)) < 0)
	return -1;

    if (raw_enable_permisc(s, iface_alias) < 0)
	return -1;
    
    return s;
}
