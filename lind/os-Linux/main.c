#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include <archenv.h>
#include <linuxsyscall.h>
#include <lib/net.h>

#define PGSIZE 4096
static const uint64_t phy_pages = 512;

void linux_main(int ac, char **av);

static char the_if[] = "eth0";

static char local_ip[]  = "171.66.3.240";
static char gw_ip[]     = "171.66.3.1";
static char gw_dst[]    = "171.66.3.0";
static char gw_nm[]     = "255.255.255.0";

int bench_client(char *ip, unsigned short port);
void bench_server(unsigned short port);

void 
net_test(void)
{
    int s;
    s = linux_socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("linux_socket: error: %d\n", s);
        assert(0);
    }

    assert(if_up(s, the_if, inet_addr(local_ip)) == 0);

    printf("* adding routes\n");
    assert(rt_add_gw(s, inet_addr(gw_ip), inet_addr(gw_dst), 
		     inet_addr(gw_nm), the_if) == 0);
    
    linux_close(s);
    printf("* running bench client\n");
    bench_client("171.66.3.151", 9999);
    //printf("* running bench server\n");
    //bench_server(9999);
    
    assert(0);
}

int
main(int ac, char **av)
{
    uint64_t phy_bytes = phy_pages * PGSIZE;
    void *va  = memalign(PGSIZE, phy_bytes);
    if (!va) {
	printf("unable to alloc segment: %s", strerror(errno));
	return -1;
    }

    arch_env.phy_start = (unsigned long) va;
    arch_env.phy_bytes = (unsigned long) phy_bytes;
    
    printf("starting linux: phy_start 0x%lx phy_end 0x%lx\n",
	   arch_env.phy_start, arch_env.phy_start + arch_env.phy_bytes);

    linux_main(ac, av);
    printf("Returned from Linux kernel!\n");
    return -1;
}
