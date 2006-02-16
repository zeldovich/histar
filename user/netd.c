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
static int netd_debug = 0;
static int netd_force_taint = 1;

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

static struct ulabel *
force_taint_prepare(uint64_t taint)
{
    struct ulabel *l = label_get_current();
    assert(l);

    level_t taint_level = netd_force_taint ? 3 : LB_LEVEL_STAR;
    assert(0 == label_set_level(l, taint, taint_level, 1));

    segment_set_default_label(l);
    int r = heap_relabel(l);
    if (r < 0)
	panic("cannot relabel heap: %s", e2s(r));

    return l;
}

static void
force_taint_commit(struct ulabel *l)
{
    int r = label_set_current(l);
    if (r < 0)
	panic("cannot reset label to %s: %s", label_to_string(l), e2s(r));

    if (netd_debug)
	printf("netd: switched to label %s\n", label_to_string(l));
}

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s grant-handle taint-handle\n", av[0]);
	return -1;
    }

    uint64_t grant, taint;
    int r = strtoull(av[1], 0, 10, &grant);
    if (r < 0)
	panic("parsing grant handle %s: %s", av[1], e2s(r));

    r = strtoull(av[2], 0, 10, &taint);
    if (r < 0)
	panic("parsing taint handle %s: %s", av[2], e2s(r));

    if (netd_debug)
	printf("netd: grant handle %ld, taint handle %ld\n",
	       grant, taint);

    struct ulabel *l = force_taint_prepare(taint);
    netd_server_init(start_env->root_container, start_env->container, l);
    force_taint_commit(l);

    struct netif nif;
    lwip_init(&nif);
    dhcp_start(&nif);

    container = start_env->container;
    struct cobj_ref receive_thread;
    r = thread_create(container, &net_receive, &nif, &receive_thread,
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

    printf("netd: ready\n");

    netd_server_ready();
    sys_thread_halt();
}
