#ifndef JOS_LWIP_LWIPOPTS_H
#define JOS_LWIP_LWIPOPTS_H

#define cprintf printf

#define LWIP_DHCP		1
#define LWIP_COMPAT_SOCKETS	0
#define LWIP_STATS_DISPLAY	1
#define SYS_LIGHTWEIGHT_PROT	1

// Various tuning knobs, partly from jos32's lwipopts.h
#define MEM_ALIGNMENT		4

#define MEMP_SANITY_CHECK	1
#define MEMP_NUM_PBUF		64
#define MEMP_NUM_UDP_PCB	8
#define MEMP_NUM_TCP_PCB	128
#define MEMP_NUM_TCP_PCB_LISTEN	16
#define MEMP_NUM_TCP_SEG	36
#define MEMP_NUM_NETBUF		128
#define MEMP_NUM_NETCONN	32
#define MEMP_NUM_API_MSG        64
#define MEMP_NUM_TCPIP_MSG      16

#define PER_TCP_PCB_BUFFER	(16 * 4096)
#define MEM_SIZE		(PER_TCP_PCB_BUFFER*MEMP_NUM_TCP_SEG + 4096*MEMP_NUM_TCP_SEG)

#define PBUF_POOL_SIZE		512
#define PBUF_POOL_BUFSIZE	2000

#define TCP_MSS			1440
#define TCP_SND_BUF		(42 * TCP_MSS)
#define TCP_SND_QUEUELEN	(3 * TCP_SND_BUF/TCP_MSS)
#define TCP_WND			(32 * TCP_MSS)

// Print error messages when we run out of memory
#define LWIP_DEBUG	1
#define MEMP_DEBUG	DBG_ON
#define DBG_TYPES_ON	DBG_ON
#define DBG_MIN_LEVEL	DBG_LEVEL_SERIOUS

#endif
