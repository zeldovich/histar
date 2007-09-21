#include <lwip/netif.h>

void	tun_input(struct netif *netif);
err_t	tun_init(struct netif *netif);

struct tun_if {
    int fd;
};
