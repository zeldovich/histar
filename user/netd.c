#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>

#include <lwip/netif.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/dhcp.h>
#include <netif/etharp.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>

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
    int r = thread_create(container, &net_timer, t, &t->thread);
    if (r < 0)
	panic("cannot create timer thread: %s", e2s(r));
}

static void
netd_server()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(23);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    cprintf("netd: server on port 23\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            cprintf("cannot accept client: %d\n", ss);
            continue;
        }

        char *msg = "Hello world.\n";
        write(ss, msg, strlen(msg));
        close(ss);
    }
}

static void
tcpip_init_done(void *arg)
{
    sys_sem_t *sem = arg;
    sys_sem_signal(*sem);
}

int
main(int ac, char **av)
{
    // container is passed as argument to _start()
    container = start_arg;

    struct netif nif;
    lwip_init(&nif);
    dhcp_start(&nif);

    struct cobj_ref receive_thread;
    int r = thread_create(container, &net_receive, &nif, &receive_thread);
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

    cprintf("netd: running\n");

    netd_server();
    sys_thread_halt();    
}
