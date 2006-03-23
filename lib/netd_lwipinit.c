#include <inc/assert.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <arch/sys_arch.h>
#include <netif/etharp.h>

#include <jif/jif.h>
#include <jif/tun.h>

// various netd initialization and threads
static int netd_stats = 0;

struct timer_thread {
    int msec;
    void (*func)(void);
    const char *name;
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
    struct netif *nif = (struct netif *) arg;

    lwip_core_lock();
    for (;;)
	jif_input(nif);
}

static void __attribute__((noreturn))
net_timer(void *arg)
{
    struct timer_thread *t = (struct timer_thread *) arg;

    for (;;) {
	uint64_t cur = sys_clock_msec();

	lwip_core_lock();
	t->func();
	lwip_core_unlock();

	uint64_t v = 0xabcd;
	sys_sync_wait(&v, v, cur + t->msec);
    }
}

static void
start_timer(struct timer_thread *t, void (*func)(void), const char *name, int msec)
{
    t->msec = msec;
    t->func = func;
    t->name = name;
    int r = thread_create(start_env->proc_container, &net_timer,
			  t, &t->thread, name);
    if (r < 0)
	panic("cannot create timer thread: %s", e2s(r));
}

static void
tcpip_init_done(void *arg)
{
    sys_sem_t *sem = (sys_sem_t *) arg;
    sys_sem_signal(*sem);
}

void
netd_lwip_init(void (*cb)(void *), void *cbarg)
{
    lwip_core_lock();

    struct netif nif;
    lwip_init(&nif);
    dhcp_start(&nif);

    struct cobj_ref receive_thread;
    int r = thread_create(start_env->proc_container, &net_receive, &nif,
			  &receive_thread, "rx thread");
    if (r < 0)
	panic("cannot create receiver thread: %s", e2s(r));

    struct timer_thread t_arp, t_tcpf, t_tcps, t_dhcpf, t_dhcpc;

    start_timer(&t_arp,	    &etharp_tmr,	"arp timer",	ARP_TMR_INTERVAL);
    start_timer(&t_tcpf,    &tcp_fasttmr,	"tcp f timer",	TCP_FAST_INTERVAL);
    start_timer(&t_tcps,    &tcp_slowtmr,	"tcp s timer",	TCP_SLOW_INTERVAL);
    start_timer(&t_dhcpf,   &dhcp_fine_tmr,	"dhcp f timer",	DHCP_FINE_TIMER_MSECS);
    start_timer(&t_dhcpc,   &dhcp_coarse_tmr,	"dhcp c timer",	DHCP_COARSE_TIMER_SECS * 1000);

    sys_sem_t tcpip_init_sem = sys_sem_new(0);
    tcpip_init(&tcpip_init_done, &tcpip_init_sem);
    sys_sem_wait(tcpip_init_sem);
    sys_sem_free(tcpip_init_sem);

    cb(cbarg);

    int dhcp_state = 0;
    const char *dhcp_states[] = {
	[DHCP_SELECTING] "selecting",
	[DHCP_CHECKING] "checking",
	[DHCP_BOUND] "bound",
    };

    for (;;) {
	if (dhcp_state != nif.dhcp->state) {
	    dhcp_state = nif.dhcp->state;
	    cprintf("netd: DHCP state %d (%s)\n", dhcp_state,
		    dhcp_states[dhcp_state] ? : "unknown");
	}

	if (netd_stats)
	    stats_display();

	lwip_core_unlock();
	thread_sleep(1000);
	lwip_core_lock();
    }
}
