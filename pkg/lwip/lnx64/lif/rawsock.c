#include <lif/rawsock.h>

#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

int
raw_socket(void)
{
    return socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
}
