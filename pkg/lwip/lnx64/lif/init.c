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

#include <api/ext.h>

#include <lif/init.h>
#include <lif/rawsock.h>
#include <lif/fd.h>


#include <pthread.h>
#include <unistd.h>
#include <errno.h>

// various netd initialization and threads
static int netd_stats = 0;

// XXX
void usleep(unsigned long usec);

static struct netif the_nif;
static int64_t the_sock;

struct timer_thread t_arp, t_tcpf, t_tcps, t_dhcpf, t_dhcpc;
struct ip_addr ipaddr, netmask, gateway;

struct timer_thread {
    int msec;
    void (*func)(void);
    const char *name;
    pthread_t tid;
};

static void * __attribute__((noreturn))
net_receive(void *arg)
{
    struct netif *nif = (struct netif *) arg;
    
    lwip_core_lock();
    for (;;)
	fd_input(nif);
}

static void
tcpip_init_done(void *arg)
{
    sys_sem_t *sem = (sys_sem_t *) arg;
    sys_sem_signal(*sem);
}

static void *__attribute__((noreturn))
net_timer(void *arg)
{
    struct timer_thread *t = (struct timer_thread *) arg;

    for (;;) {
	lwip_core_lock();
	t->func();
	lwip_core_unlock();
	usleep(t->msec * 1000);
    }
}

static void
start_timer(struct timer_thread *t, void (*func)(void), const char *name, int msec)
{
    t->msec = msec;
    t->func = func;
    t->name = name;
    
    int r = pthread_create(&t->tid, 0, &net_timer, t);
    if (r < 0)
	lwip_panic("cannot create timer thread: %s", strerror(errno));
}

int __attribute__((noreturn))
lwip_init(void (*cb)(void *), void *cbarg, const char* iface_alias)
{
    if ((the_sock = raw_socket(iface_alias)) < 0)
	lwip_panic("couldn't open raw socket: %s\n", strerror(errno));

    printf("lwip_init: the_sock %d\n", the_sock);
    
    lwip_core_lock();
    
    lwipext_init(0);
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

    uint32_t init_addr = 0;
    uint32_t init_mask = 0;
    uint32_t init_gw = 0;

    ipaddr.addr  = init_addr;
    netmask.addr = init_mask;
    gateway.addr = init_gw;
    
    if (0 == netif_add(&the_nif, &ipaddr, &netmask, &gateway,
		       (uint64_t *)the_sock,
		       fd_init,
		       ip_input))
	lwip_panic("lwip_init: error in netif_add\n");
    
    netif_set_default(&the_nif);
    netif_set_up(&the_nif);

    dhcp_start(&the_nif);

    pthread_t receive_thread;
    int r = pthread_create(&receive_thread, 0, &net_receive, &the_nif);
    if (r < 0)
	lwip_panic("cannot create reciever thread: %s\n", strerror(errno));
    
    start_timer(&t_arp, &etharp_tmr, "arp timer", ARP_TMR_INTERVAL);
    start_timer(&t_tcpf, &tcp_fasttmr, "tcp f timer", TCP_FAST_INTERVAL);
    start_timer(&t_tcps, &tcp_slowtmr, "tcp s timer", TCP_SLOW_INTERVAL);

    start_timer(&t_dhcpf, &dhcp_fine_tmr,	"dhcp f timer",	DHCP_FINE_TIMER_MSECS);
    start_timer(&t_dhcpc, &dhcp_coarse_tmr,	"dhcp c timer",	DHCP_COARSE_TIMER_SECS * 1000);
    
    sys_sem_t tcpip_init_sem = sys_sem_new(0);
    tcpip_init(&tcpip_init_done, &tcpip_init_sem);

    sys_sem_wait(tcpip_init_sem);
    sys_sem_free(tcpip_init_sem);

    int dhcp_state = 0;
    const char *dhcp_states[] = {
	[DHCP_RENEWING] "renewing",
	[DHCP_SELECTING] "selecting",
	[DHCP_CHECKING] "checking",
	[DHCP_BOUND] "bound",
    };

    for (;;) {

	if (dhcp_state != the_nif.dhcp->state) {
	    dhcp_state = the_nif.dhcp->state;
	    printf("netd: DHCP state %d (%s)\n", dhcp_state,
		   dhcp_states[dhcp_state] ? : "unknown");
	}

	if (netd_stats) {
	    stats_display();
	}

	lwip_core_unlock();
	usleep(1000000);
	lwip_core_lock();
    }
    
    //return 0;
}
