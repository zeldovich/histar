#include <sys/syscall.h>
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
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#include "rawsock.h"
#include <os-Linux/util.h>

// XXX
static struct ifreq ifr_cleanup;

static void
raw_cleanup(void)
{
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("raw_cleanup: socket failed");
	return;
    }

    if (ioctl(s, SIOCSIFFLAGS, &ifr_cleanup) < 0)
	printf("raw_cleanup: ioctl error: %s\n", strerror(errno));

    close(s);
}

static int
raw_enable_permisc(int s, const char *iface_alias)
{
    struct ifreq ifr;
    struct packet_mreq pr;

    strncpy(ifr.ifr_name, iface_alias, IFNAMSIZ);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
	printf("ioctl error: %s", strerror(errno));
	return -1;
    }

    // why doesn't the 'new' way work?
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
raw_enable_filter(int s, const unsigned char *mac_addr)
{
    uint32_t mac0 = 0;
    uint32_t mac1 = 0;
    struct sock_filter BPF_code[13];
    struct sock_fprog filter; 

    memcpy(&mac1, &mac_addr[2], 4);
    memcpy(&mac0, &mac_addr[0], 2);
    mac1 = htonl(mac1);
    mac0 = htons(mac0);
    
    // -s 0 ether host AA:BB:CC:DD:EE:FF or ether broadcast
    BPF_code[0] = (struct sock_filter) { 0x20, 0, 0, 0x00000008 };
    BPF_code[1] = (struct sock_filter) { 0x15, 0, 2, mac1 };
    BPF_code[2] = (struct sock_filter) { 0x28, 0, 0, 0x00000006 };
    BPF_code[3] = (struct sock_filter) { 0x15, 7, 0, mac0 };
    BPF_code[4] = (struct sock_filter) { 0x20, 0, 0, 0x00000002 };
    BPF_code[5] = (struct sock_filter) { 0x15, 0, 2, mac1 };
    BPF_code[6] = (struct sock_filter) { 0x28, 0, 0, 0x00000000 };
    BPF_code[7] = (struct sock_filter) { 0x15, 3, 4, mac0 };
    BPF_code[8] = (struct sock_filter) { 0x15, 0, 3, 0xffffffff };
    BPF_code[9] = (struct sock_filter) { 0x28, 0, 0, 0x00000000 };
    BPF_code[10] = (struct sock_filter) { 0x15, 0, 1, 0x0000ffff };
    BPF_code[11] = (struct sock_filter) { 0x6, 0, 0, 0x0000ffff };
    BPF_code[12] = (struct sock_filter) { 0x6, 0, 0, 0x00000000 };
    
    filter.len = 13;
    filter.filter = BPF_code;
	
    if(setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, 
		  &filter, sizeof(filter)) < 0) {
	printf("setsockopt error: %s\n", strerror(errno));
	return -1;
    }

    return 0;
}

static int
raw_socket(const char *iface_alias, const unsigned char *mac_addr)
{
    struct ifreq ifr;
    struct sockaddr_ll sll;
    int s, tmp;
    s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (s < 0) {
	perror("raw_socket");
	return s;
    }
    
    /* a temporary socket for setuping up the if */
    tmp = socket(PF_INET, SOCK_DGRAM, 0);
    if (tmp < 0) {
	close(s);
	return -errno;
    }
    
    if (raw_enable_filter(s, mac_addr) < 0)
	return -1;
    
    strncpy(ifr.ifr_name, iface_alias, IFNAMSIZ);
    if (ioctl(tmp, SIOCGIFINDEX, &ifr) < 0)
	return -1;
    
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(s, (struct sockaddr *) &sll, sizeof(sll)) < 0)
	return -1;

    if (raw_enable_permisc(tmp, iface_alias) < 0)
	return -1;
    
    close(tmp);
    return s;
}

static unsigned char the_mac[] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x10 };

int 
rawsock_mac(unsigned char *buf)
{
    memcpy(buf, the_mac, sizeof(the_mac));
    return 0;
}

int
rawsock_open(const char *name, void *data)
{
    /* XXX no error handling (in any functions) */
    int s, r, flags;
    struct rawsock_data *rd = data;

    s = raw_socket("eth1", the_mac);
    if (s < 0)
	return -1;
    
    if ((flags = fcntl(s, F_GETFL)) < 0)
	return -errno;
    
    flags |= O_ASYNC | O_NONBLOCK;
    
    if ((r = fcntl(s, F_SETFL, flags)) < 0)
	return -errno;

    if ((r = fcntl(s, F_SETOWN, util_getpid())) < 0)
	return -errno;
        
    if ((r = fcntl(s, F_SETSIG, SIGIO)) < 0)
	return -errno;

    rd->sockfd = s;

    return 0;
}

int
rawsock_tx(void *data, void *buf, unsigned int buf_len)
{
    int r;
    struct rawsock_data *rd = data;
    
    r = write(rd->sockfd, buf, buf_len);
    if (r < 0) {
	perror("rawsock_output: write failed");
	return -1;
    } else if (r != buf_len) {
	printf("rawsock_output: write truncate: %d -> %d\n", buf_len, r);
	return r;
    }

    return r;
}

int
rawsock_rx(void *data, void *buf, unsigned int buf_len)
{
    int r;
    struct rawsock_data *rd = data;
    
    r = read(rd->sockfd, buf, buf_len);
    if (r < 0) {
	if (errno == EAGAIN)
	    return 0;
	perror("rawsock_output: read failed");
	return -1;
    }

    return r;
}
