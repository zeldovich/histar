#include <inc/lib.h>
#include <inc/assert.h>
#include <machine/memlayout.h>
#include <machine/x86.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include <archenv.h>
#include <longjmp.h>
#include <linuxsyscall.h>
#include <archcall.h>
#include <os-lib/net.h>
#include <os-lib/netd.h>
#include <os-jos64/kernelcall.h>
#include <os-jos64/lfs.h>
#include <linuxthread.h>

static const uint64_t phy_pages = 512;

void linux_main(int ac, char **av);

int bench_client(char *ip, unsigned short port);
void bench_server(unsigned short port);

static int
kernel_call_stub(void *x)
{
    unsigned long mask = 1 << (SIGUSR1 - 1);
    linux_sigprocmask(SIG_UNBLOCK, &mask, 0);
    kernel_call_init();
    return 0;
}

static void __attribute__((unused))
setup_static_ip(void)
{
    static char the_if[] = "netdev";
    
    static char local_ip[]  = "171.66.3.230";
    static char gw_ip[]     = "171.66.3.1";
    static char gw_dst[]    = "171.66.3.0";
    static char gw_nm[]     = "255.255.255.0";
    
    int s = linux_socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        panic("linux_socket: error: %d", s);

    printf("* enabling interface %s\n", the_if);
    assert(if_up(s, the_if, inet_addr(local_ip)) == 0);
    printf("* adding routes\n");
    assert(rt_add_gw(s, inet_addr(gw_ip), inet_addr(gw_dst), 
    		     inet_addr(gw_nm), the_if) == 0);
    linux_close(s);
}

void
main_loop(void)
{
    int r;
    
    r = netd_linux_init();
    if (r < 0)
	panic("netd_init: error: %d", r);

    lfs_init();
    linux_thread_run(kernel_call_stub, 0, "kcall");
    netd_linux_main();    
}

int
main(int ac, char **av)
{
    struct cobj_ref seg;
    void *va = 0;
    uint64_t phy_bytes = phy_pages * PGSIZE;
    int r = segment_alloc(start_env->shared_container, phy_bytes,
			  &seg, &va, 0, "physical-memory");
    if (r < 0)
	panic("unable to alloc segment: %s", e2s(r));
    memset(va, 0, phy_bytes);

    arch_env.phy_start = (unsigned long) va;
    arch_env.phy_bytes = (unsigned long) phy_bytes;
    
    printf("starting linux: phy_start 0x%lx phy_end 0x%lx\n",
	   arch_env.phy_start, arch_env.phy_start + arch_env.phy_bytes);
    linux_main(ac, av);
        
    return 0;
}
