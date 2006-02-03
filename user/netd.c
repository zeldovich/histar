#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>

#include <lwip/netif.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>

#include <jif/jif.h>

static uint64_t container;

struct timer_thread {
    int msec;
    void (*func)();
    struct cobj_ref thread;
};

static void
lwip_init(struct netif *nif)
{
    // lwIP initialization sequence, as suggested by lwip/doc/rawapi.txt
    stats_init();
    sys_init();
    mem_init();
    memp_init();
    pbuf_init();
    etharp_init();
    ip_init();
    udp_init();
    tcp_init();

    struct ip_addr ipaddr, netmask, gateway;
    memset(&ipaddr,  0, sizeof(ipaddr));
    memset(&netmask, 0, sizeof(netmask));
    memset(&gateway, 0, sizeof(gateway));

    if (0 == netif_add(nif, &ipaddr, &netmask, &gateway, 0, jif_init, ip_input))
	panic("lwip_init: error in netif_add\n");

    netif_set_default(nif);
    netif_set_up(nif);
}

static void __attribute__((noreturn))
net_receive(void *arg)
{
    struct netif *nif = arg;

    for (;;)
	jif_input(nif);
}

static void __attribute__((noreturn))
net_timer(void *arg)
{
    struct timer_thread *t = arg;

    for (;;) {
	t->func();
	sys_thread_sleep(t->msec);
    }
}

static void
start_timer(struct timer_thread *t, void (*func)(), int msec)
{
    t->msec = msec;
    t->func = func;
    int r = thread_create(container, &net_timer, t, &t->thread, "timer thread");
    if (r < 0)
	panic("cannot create timer thread: %s", e2s(r));
}

static void
tcpip_init_done(void *arg)
{
    sys_sem_t *sem = arg;
    sys_sem_signal(*sem);
}

static void
netdev_init(uint64_t ct)
{
    int64_t netdev_id = container_find(ct, kobj_netdev, 0);
    if (netdev_id < 0) {
	struct ulabel ul = { .ul_default = 1, .ul_nent = 0 };
	netdev_id = sys_net_create(ct, &ul);
	if (netdev_id < 0)
	    panic("cannot create netdev: %s", e2s(netdev_id));
    }
}

int
main(int ac, char **av)
{
    // container is passed as argument to _start()
    container = start_env->container;

    netdev_init(start_env->root_container);

    struct netif nif;
    lwip_init(&nif);
    dhcp_start(&nif);

    struct cobj_ref receive_thread;
    int r = thread_create(container, &net_receive, &nif, &receive_thread,
			  "rx thread");
    if (r < 0)
	panic("cannot create receiver thread: %s", e2s(r));

    struct timer_thread t_arp, t_tcpf, t_tcps, t_dhcpf, t_dhcpc;

    start_timer(&t_arp,	    &etharp_tmr,	ARP_TMR_INTERVAL);
    start_timer(&t_tcpf,    &tcp_fasttmr,	TCP_FAST_INTERVAL);
    start_timer(&t_tcps,    &tcp_slowtmr,	TCP_SLOW_INTERVAL);
    start_timer(&t_dhcpf,   &dhcp_fine_tmr,	DHCP_FINE_TIMER_MSECS);
    start_timer(&t_dhcpc,   &dhcp_coarse_tmr,	DHCP_COARSE_TIMER_SECS * 1000);

    sys_sem_t tcpip_init_sem = sys_sem_new(0);
    tcpip_init(&tcpip_init_done, &tcpip_init_sem);
    sys_sem_wait(tcpip_init_sem);
    sys_sem_free(tcpip_init_sem);

    printf("netd: running\n");

    netd_server_init(container);
    sys_thread_halt();
}
