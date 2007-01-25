#include <inc/lib.h>
#include <inc/time.h>
#include <inc/ntp.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/syscall.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>

enum { poll_time = 300 };
static int cur_delay;

static uint64_t
ntp_ts_to_msec(ntp_ts ts)
{
    uint64_t msec_frac = (UINT64(1) << 32) / 1000;
    return (CAST64(ntohl(ts.ts_sec)) * 1000) +
	   (ntohl(ts.ts_frac) / msec_frac);
}

static void __attribute__((noreturn))
receiver(void *arg)
{
    int *fdp = arg;
    int fd = *fdp;

    struct time_of_day_seg *tods = 0;
    int r = segment_map(start_env->time_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &tods, 0, 0);
    if (r < 0)
	panic("cannot map time-of-day segment: %s\n", e2s(r));

    for (;;) {
	union {
	    char msgbuf[1024];
	    struct ntp_packet pkt;
	} u;

	int cc = recv(fd, &u, sizeof(u), 0);
	if (cc <= 0) {
	    perror("recv");
	    continue;
	}

	if ((uint32_t) cc < sizeof(u.pkt)) {
	    printf("jntpd receiver: short packet, %d < %ld\n", cc, sizeof(u.pkt));
	    continue;
	}

	if (NTP_LVM_MODE(u.pkt.ntp_lvm) != NTP_MODE_SERVER ||
	    !u.pkt.ntp_stratum || u.pkt.ntp_stratum > 15) {
	    printf("jntpd receiver: stray server..\n");
	    continue;
	}

	uint64_t t4 = sys_clock_msec();
	uint64_t t3 = ntp_ts_to_msec(u.pkt.ntp_transmit_ts);
	uint64_t t2 = ntp_ts_to_msec(u.pkt.ntp_receive_ts);
	uint64_t t1 = ntp_ts_to_msec(u.pkt.ntp_originate_ts);
	uint64_t delay = ((t4 - t1) - (t3 - t2))/2;
	//printf("reply: t1=%ld, t2=%ld, t3=%ld, t4=%ld\n", t1, t2, t3, t4);

	uint64_t unix_msec_at_t4 = (t2 + t3) / 2 + delay -
				   UINT64(2208988800) * 1000;
	tods->unix_msec_offset = unix_msec_at_t4 - t4;
	cur_delay = poll_time;

	static int synced;
	if (!synced) {
	    printf("jntpd: synchronized local time\n");
	    synced = 1;
	}
    }
}

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s ntp-server\n", av[0]);
	exit(-1);
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(-1);
    }

    struct sockaddr_in sin;
    int dns_delay = 1;
    for (;;) {
	struct hostent *he = gethostbyname(av[1]);
	if (!he) {
	    sleep(dns_delay);
	    dns_delay *= 2;
	    if (dns_delay > poll_time)
		dns_delay = poll_time;
	    continue;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr, he->h_addr, sizeof(sin.sin_addr));
	sin.sin_port = htons(123);
	break;
    }

    if (connect(fd, &sin, sizeof(sin)) < 0) {
	perror("connect");
	exit(-1);
    }

    struct cobj_ref tid;
    int r = thread_create(start_env->proc_container, &receiver,
			  &fd, &tid, "receiver");
    if (r < 0) {
	printf("thread_create: %s\n", e2s(r));
	exit(-1);
    }

    cur_delay = 1;
    for (;;) {
	struct ntp_packet ntp;
	memset(&ntp, 0, sizeof(ntp));
	ntp.ntp_lvm = NTP_LVM_ENCODE(NTP_LI_NONE, 4, NTP_MODE_CLIENT);

	uint64_t ts = sys_clock_msec();
	uint64_t msec_frac = (UINT64(1) << 32) / 1000;
	ntp.ntp_transmit_ts.ts_sec = htonl(ts / 1000);
	ntp.ntp_transmit_ts.ts_frac = htonl((ts % 1000) * msec_frac);

	//printf("Sending a request: ts=%ld\n", ts);
	send(fd, &ntp, sizeof(ntp), 0);

	sleep(cur_delay);
	cur_delay *= 2;
	if (cur_delay > poll_time)
	    cur_delay = poll_time;
    }
}
