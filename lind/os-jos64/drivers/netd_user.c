#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/jcomm.h>
#include <inc/bipipe.h>
#include <inc/netd.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/multisync.h>
#include <inc/stdio.h>
#include <inc/debug.h>
#include <inc/setjmp.h>
#include <inc/string.h>
#include <netd/netdlinux.h>

#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#include <os-jos64/lutrap.h>
#include <os-lib/netd.h>
#include <linuxsyscall.h>
#include <linuxthread.h>
#include <archcall.h>

#include "netduser.h"

/* copied from linux/errno.h */
#define ERESTARTNOHAND  514     /* restart if no handler.. */

enum { dbg = 0 };
enum { netd_do_taint = 0 };

static uint64_t inet_taint;
static int linux_main_pid;

static int linux_socket_thread(void *a);

#define NETD_OP_ENTRY(name) [netd_op_##name] = #name,
const char *netd_op_name[] = {
    ALL_NETD_OPS
};
#undef NETD_OP_ENTRY

static void
netd_to_libc(struct netd_sockaddr_in *nsin, struct sockaddr_in *sin)
{
    sin->sin_family = AF_INET;
    sin->sin_port = nsin->sin_port;
    sin->sin_addr.s_addr = nsin->sin_addr;
}

static void
libc_to_netd(struct sockaddr_in *sin, struct netd_sockaddr_in *nsin)
{
    nsin->sin_addr = sin->sin_addr.s_addr;
    nsin->sin_port = sin->sin_port;
}

static void
addthread_slot(struct sock_slot *s, void *x)
{
    if (s->linuxpid == 0)
	linux_thread_run(linux_socket_thread, s, "socket-thread");
}

static void
service_slot(struct sock_slot *s, void *a)
{
    int r;
    /* XXX if the current_thread_info() is for the idle thread, Linux
     * will complain if we try to make a thread, so we wake up the 
     * original thread and have it create the thread...Better way to 
     * switch threads (or at least current_thread_infos()?
     */
    char *wake_main_thread = (char *) a;
    if (s->linuxpid == 0)
	*wake_main_thread = 1;

    if (s->jos2lnx_full || s->outcnt || s->lnx2jos_full == CNT_LIMBO) {
	r = linux_kill(s->linuxpid, SIGUSR1);
	if (r < 0)
	    panic("unable to kill: %d", r);
    }
}

void 
netd_user_interrupt(void)
{
    char wake_main_thread = 0;
    slot_for_each(service_slot, &wake_main_thread);
    if (wake_main_thread)
	linux_kill(linux_main_pid, SIGUSR1);
}

void
netd_user_set_taint(const char *str)
{
    int r = strtou64(str, 0, 10, &inet_taint);
    if (r < 0)
	panic("unable to parse taint: %s\n", str);
}

static int
netd_linux_ioctl(struct sock_slot *ss, struct netd_op_ioctl_args *a)
{
    int r;

    debug_print(dbg, "(l%ld) ioctl %ld", ss->linuxpid, a->libc_ioctl);
    
    switch(a->libc_ioctl) {
    case SIOCGIFCONF: {
	struct ifconf ifc;
	struct ifreq *ifrp;
	static const uint32_t maxifs = sizeof(a->gifconf.ifs) /
				       sizeof(a->gifconf.ifs[0]);
	struct ifreq buf[maxifs];
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = (void *) &buf[0];

	if ((r = linux_ioctl(ss->sock, SIOCGIFCONF, &ifc)) < 0)
	    return r;

	uint32_t nifs = ifc.ifc_len / sizeof(struct ifreq);
	uint32_t i;
	for (i = 0; i < nifs; i++) {
	    ifrp = &buf[i];
	    uint32_t n = MIN(sizeof(a->gifconf.ifs[i].name),
			     sizeof(ifrp->ifr_name));
	    strncpy(a->gifconf.ifs[i].name, ifrp->ifr_name, n);
	    a->gifconf.ifs[i].name[n] = '\0';
	    libc_to_netd((struct sockaddr_in *)&ifrp->ifr_addr,
			 &a->gifconf.ifs[i].addr);
	}

	a->gifconf.ifcount = nifs;
	return r;
    }
    case SIOCGIFFLAGS: {
	struct ifreq ifrp;
	strncpy(ifrp.ifr_name, a->gifflags.name, sizeof(ifrp.ifr_name));
	ifrp.ifr_name[sizeof(ifrp.ifr_name) - 1] = 0;

	if ((r = linux_ioctl(ss->sock, SIOCGIFFLAGS, &ifrp)) < 0)
	    return r;
	
	a->gifflags.flags = ifrp.ifr_flags;
	return r;
    }
    case SIOCGIFHWADDR: {
	struct ifreq ifrp;
	int family, len;
	strncpy(ifrp.ifr_name, a->gifhwaddr.name, sizeof(ifrp.ifr_name));
	ifrp.ifr_name[sizeof(ifrp.ifr_name) - 1] = 0;
	
	if ((r = linux_ioctl(ss->sock, SIOCGIFHWADDR, &ifrp)) < 0)
	    return r;
	
	family = ifrp.ifr_hwaddr.sa_family;
	len = 6;
	if (family != ARPHRD_ETHER) {
	    arch_printf("gifhwaddr: unsupported family %d\n", family);
	    return -ENOSYS;
	}
	a->gifhwaddr.hwfamily = family;
	a->gifhwaddr.hwlen = len;
	memcpy(a->gifhwaddr.hwaddr, ifrp.ifr_hwaddr.sa_data, len);
	return r;
    }
    case SIOCGIFBRDADDR: {
	struct ifreq ifrp;
	strncpy(ifrp.ifr_name, a->gifbrdaddr.name, sizeof(ifrp.ifr_name));
	ifrp.ifr_name[sizeof(ifrp.ifr_name) - 1] = '\0';

	if ((r = linux_ioctl(ss->sock, SIOCGIFBRDADDR, &ifrp)) < 0)
	    return r;

	libc_to_netd((struct sockaddr_in *)&ifrp.ifr_addr,
		     &a->gifbrdaddr.baddr);
	return r;
    }
    default:
	arch_printf("netd_linux_ioctl: unimplemented %d\n", a->libc_ioctl);
	return -ENOSYS;
    }
    return -ENOSYS;
} 

static void
netd_linux_dispatch(struct sock_slot *ss, struct netd_op_args *a)
{
    int r;
    struct sockaddr_in sin;
    int sinlen = sizeof(sin);
    int xlen;

    debug_print(dbg, "(l%ld) op %d (%s), sock %d",
		ss->linuxpid, a->op_type,
		(a->op_type < netd_op_max ? netd_op_name[a->op_type] : "unknown"),
		ss->sock);

    switch(a->op_type) {
    case netd_op_socket:
	r = linux_socket(a->socket.domain, 
			 a->socket.type, 
			 a->socket.protocol);	
	if (r >= 0) {
	    ss->sock = r;
	    ss->lnx2jos_full = 0;
	    a->rval = slot_to_id(ss);
	} else
	    a->rval = r;
	break;

    case netd_op_close:
	r = linux_close(ss->sock);
	if (r < 0)
	    arch_printf("netd_linux_dispatch: close error: %d\n", r);
	a->rval = r;
	break;

    case netd_op_bind:
	netd_to_libc(&a->bind.sin, &sin);
	a->rval = linux_bind(ss->sock, (struct sockaddr *)&sin, sinlen);
	break;

    case netd_op_listen:
	a->rval = linux_listen(ss->sock, a->listen.backlog);
	if (a->rval == 0) {
	    ss->listen = 1;
	    ss->lnx2jos_full = 0;
	}
	break;

    case netd_op_connect:
	netd_to_libc(&a->connect.sin, &sin);
	a->rval = linux_connect(ss->sock, (struct sockaddr *)&sin, sinlen);
	if (a->rval == 0)
	    ss->lnx2jos_full = 0;
	break;

    case netd_op_send:
	a->rval = linux_send(ss->sock, a->send.buf, a->send.count, a->send.flags);
	break;

    case netd_op_sendto:
	netd_to_libc(&a->sendto.sin, &sin);
	a->rval = linux_sendto(ss->sock, a->sendto.buf, a->sendto.count,
			       a->sendto.flags, (struct sockaddr *)&sin, sinlen);
	break;

    case netd_op_ioctl:
	a->rval = netd_linux_ioctl(ss, &a->ioctl);
	break;

    case netd_op_setsockopt:
	a->rval = linux_setsockopt(ss->sock, a->setsockopt.level,
				   a->setsockopt.optname,
				   a->setsockopt.optval,
				   a->setsockopt.optlen);
	break;

    case netd_op_getsockopt:
	xlen = MIN(sizeof(a->getsockopt.optval), a->getsockopt.optlen);
	a->rval = linux_getsockopt(ss->sock, a->getsockopt.level,
				   a->getsockopt.optname,
				   a->getsockopt.optval,
				   &xlen);
	a->getsockopt.optlen = xlen;
	break;

    case netd_op_getsockname:
	a->rval = linux_getsockname(ss->sock, (struct sockaddr *) &sin, &sinlen);
	libc_to_netd(&sin, &a->getsockname.sin);
	break;

    case netd_op_getpeername:
	a->rval = linux_getpeername(ss->sock, (struct sockaddr *) &sin, &sinlen);
	libc_to_netd(&sin, &a->getpeername.sin);
	break;

    case netd_op_shutdown:
	a->rval = linux_shutdown(ss->sock, a->shutdown.how);
	break;

    default:
	arch_printf("netd_linux_dispatch: unimplemented %d\n", a->op_type);
	a->rval = -ENOSYS;
	break;
    }

    debug_print(dbg, "(l%ld) rval %d", ss->linuxpid, a->rval);
}

/* returns >= 0 for socket events, negative for signals.
 */
static int
linux_wait_for(struct sock_slot *ss)
{
    int r;

    if (ss->sock != -1 && ss->lnx2jos_full == 0) {
	fd_set rs;
	FD_ZERO(&rs);
	FD_SET(ss->sock, &rs);
	debug_print(dbg, "(l%ld) wait_for: select", ss->linuxpid);
	r = linux_select(ss->sock + 1, &rs, 0, 0, 0);
    } else {
	debug_print(dbg, "(l%ld) wait_for: pause", ss->linuxpid);
	r = linux_pause();
    }
    debug_print(dbg, "(l%ld) wait_for: %d", ss->linuxpid, r);

    if (r == -ERESTARTNOHAND) {
	linux_thread_flushsig();

	/*
	 * XXX what does this mysterious code segment do?
	 */
	if (ss->lnx2jos_full == CNT_LIMBO) {
	    ss->lnx2jos_full = 0;
	    sys_sync_wakeup(&ss->lnx2jos_full);
	}
    }

    return r;
}

static void
linux_handle_socket(struct sock_slot *ss)
{
    int r;

    assert(ss->lnx2jos_full == 0);
    if (ss->listen) {
	struct sock_slot *nss;
	struct sockaddr_in sin;
	int addrlen = sizeof(sin);
	
	debug_print(dbg, "(l%ld) accepting conn", ss->linuxpid);
	r = linux_accept(ss->sock, (struct sockaddr *)&sin, &addrlen);
	if (r < 0) 
	    panic("linux_accept error: %d\n", r);
	nss = slot_alloc();
	if (nss == 0)
	    panic("no slots");
	nss->sock = r;
	nss->dgram = 0;
	linux_thread_run(linux_socket_thread, nss, "socket-thread");
	
	ss->lnx2jos_buf.op_type = jos64_op_accept;
	ss->lnx2jos_buf.accept.a.fd = slot_to_id(nss);
	libc_to_netd(&sin, &ss->lnx2jos_buf.accept.a.sin);
	ss->lnx2jos_full = 1;
	sys_sync_wakeup(&ss->lnx2jos_full);
	debug_print(dbg, "(l%ld) accepted conn", ss->linuxpid);
    } else {
	debug_print(dbg, "(l%ld) reading data", ss->linuxpid);
	struct sockaddr_in sin;
	int fromlen = sizeof(sin);

	r = linux_recvfrom(ss->sock,
			   ss->lnx2jos_buf.recv.buf,
			   sizeof(ss->lnx2jos_buf.recv.buf),
			   0, (struct sockaddr *) &sin, &fromlen);
	if (r == -ENOTCONN) {
	    ss->lnx2jos_full = CNT_LIMBO;
	    return;
	} else if (r == 0) {
	    ss->lnx2jos_buf.op_type = jos64_op_shutdown;
	    ss->lnx2jos_full = 1;
	    sys_sync_wakeup(&ss->lnx2jos_full);
	    debug_print(dbg, "(l%ld) shutdown", ss->linuxpid);
	    return;
	} else if (r < 0) {
	    /* XXX map read errors to EOF? */
	    ss->lnx2jos_buf.op_type = jos64_op_shutdown;
	    ss->lnx2jos_full = 1;
	    sys_sync_wakeup(&ss->lnx2jos_full);
	    debug_print(dbg, "(l%ld) shutdown err %d", ss->linuxpid, r);
	    return;
	}

	libc_to_netd(&sin, &ss->lnx2jos_buf.recv.from);
	ss->lnx2jos_buf.op_type = jos64_op_recv;
	ss->lnx2jos_buf.recv.off = 0;
	ss->lnx2jos_buf.recv.cnt = r;
	ss->lnx2jos_full = 1;
	sys_sync_wakeup(&ss->lnx2jos_full);
	debug_print(dbg, "(l%ld) read %d data", ss->linuxpid, r);
    }
}

static int
linux_socket_thread(void *a)
{
    int r;
    struct sock_slot *ss = (struct sock_slot *)a;
    
    unsigned long mask = 1 << (SIGUSR1 - 1);
    r = linux_sigprocmask(SIG_UNBLOCK, &mask, 0);
    if (r < 0)
	panic("unable to set sig mask: %d", r);

    ss->linuxpid = linux_getpid();
    sys_sync_wakeup(&ss->linuxpid);
    debug_print(dbg, "(l%ld) starting", ss->linuxpid);
    for (;;) {
	netd_op_t op;

	r = linux_wait_for(ss);

	if (r >= 0)
	    linux_handle_socket(ss);

	if (ss->jos2lnx_full) {
	    op = ss->jos2lnx_buf.op_type;
	    netd_linux_dispatch(ss, &ss->jos2lnx_buf);
	    ss->jos2lnx_full = 0;
	    sys_sync_wakeup(&ss->jos2lnx_full);
	    if (op == netd_op_close)
		break;
	}

	if (ss->outcnt) {
	    r = linux_write(ss->sock, ss->outbuf, ss->outcnt);
	    if (r == ss->outcnt) {
		ss->outcnt = 0;
		sys_sync_wakeup(&ss->outcnt);
	    } else if (r > 0) {
		memmove(&ss->outbuf[0], &ss->outbuf[r], ss->outcnt - r);
		ss->outcnt -= r;
	    }
	}
    }
    debug_print(dbg, "(l%ld) stopping", ss->linuxpid);
    slot_free(ss);
    return 0;
}

void
netd_linux_main(void)
{
    linux_main_pid = linux_getpid();
    debug_print(dbg, "(l%d) starting", linux_main_pid);
    
    uint64_t taint = netd_do_taint ? inet_taint : 0;
    int r = netd_linux_server_init(&jos64_socket_thread, taint);

    if (r < 0) 
	panic("netd_linux_main: netd_linux_server_init error: %s\n", e2s(r));
    
    for (;;) {
	linux_pause();
	linux_thread_flushsig();
	slot_for_each(&addthread_slot, 0);
    }
}

int 
netd_linux_init(void)
{
    slot_init();
    return 0;
}
