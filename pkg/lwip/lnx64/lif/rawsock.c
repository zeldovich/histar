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
    
    return s;
}
