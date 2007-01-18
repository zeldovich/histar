#include <lif/rawsock.h>

#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

#include <linux/if_packet.h>
#include <linux/if.h>
#include <linux/filter.h>
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
	printf("raw_cleanup: ioctl error: %s\n", strerror(errno));
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

static int
raw_enable_filter(int s, char *mac_addr)
{
    uint32_t mac0 = 0;
    uint32_t mac1 = 0;
    memcpy(&mac1, &mac_addr[2], 4);
    memcpy(&mac0, &mac_addr[0], 2);
    mac1 = htonl(mac1);
    mac0 = htons(mac0);
    
    // -s 1500 ether host AA:BB:CC:DD:EE:FF or ether broadcast
    struct sock_filter BPF_code[] = {
	{ 0x20, 0, 0, 0x00000008 },
	{ 0x15, 0, 2, mac1 },
	{ 0x28, 0, 0, 0x00000006 },
	{ 0x15, 7, 0, mac0 },
	{ 0x20, 0, 0, 0x00000002 },
	{ 0x15, 0, 2, mac1 },
	{ 0x28, 0, 0, 0x00000000 },
	{ 0x15, 3, 4, mac0 },
	{ 0x15, 0, 3, 0xffffffff },
	{ 0x28, 0, 0, 0x00000000 },
	{ 0x15, 0, 1, 0x0000ffff },
	{ 0x6, 0, 0, 0x000005dc },
	{ 0x6, 0, 0, 0x00000000 },
    };
	
    struct sock_fprog filter; 
    filter.len = 13;
    filter.filter = BPF_code;
	
    if(setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, 
		  &filter, sizeof(filter)) < 0) {
	printf("setsockopt error: %s\n", strerror(errno));
	return -1;
    }

    return 0;
}

int
raw_socket(const char *iface_alias, char *mac_addr)
{
    int s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (s < 0)
	return s;
    
    if (raw_enable_filter(s, mac_addr) < 0)
	return -1;

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
