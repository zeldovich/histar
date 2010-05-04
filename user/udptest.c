#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <inc/syscall.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int
main(int argc, char **argv)
{
	if (argc != 6 && argc != 7) {
		fprintf(stderr, "usage: %s desthost destport pktsize pktpersec numpkts [echo|ack|pull]\n",
		    argv[0]);
		exit(1);
	}

	int do_echo = 0;
	int do_ack  = 0;
	int do_pull = 0;
	if (argc == 7 && strcmp(argv[6], "echo") == 0)
		do_echo = 1;
	if (argc == 7 && strcmp(argv[6], "ack") == 0)
		do_ack = 1;
	if (argc == 7 && strcmp(argv[6], "pull") == 0)
		do_pull = 1;

	char *desthost = argv[1];
	uint16_t destport = atoi(argv[2]);
	unsigned int pktsize = atoi(argv[3]);
	int pktpersec = atoi(argv[4]);
	int numpkts = atoi(argv[5]);

	int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in dst;

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port = htons(destport);
	inet_aton(desthost, &dst.sin_addr);

	fprintf(stderr, "Sending packets of length %d to %s:%d\n", pktsize, desthost, destport); 

	char *buf = malloc(pktsize);
	memset(buf, 0x55, pktsize);

	if (do_echo)
		memcpy(buf, "ECHO", 4);
	if (do_ack)
		memcpy(buf, "ACK", 3);
	if (do_pull)
		memcpy(buf, "PULL", 4);

	uint64_t usec_per_pkt = 1000000 / pktpersec;

	int j = 0;
	for (int i = 0; i < numpkts; i++) {
		uint64_t before = sys_clock_nsec() / 1000;

		sendto(s, buf, pktsize, 0, (struct sockaddr *)&dst, sizeof(dst));

		// sleep until next transmit due
		uint64_t diff_usec = (sys_clock_nsec() / 1000) - before;
		if (diff_usec < usec_per_pkt)
			usleep(usec_per_pkt - diff_usec);

		j = (j + 1) % pktpersec;
		if (i != 0 && j == 0) {
			fprintf(stderr, "\rpackets sent: %d, bytes sent: %d", pktpersec, pktpersec * pktsize);
		}
	}

	// don't want socket to go away on responses
	if (do_echo || do_ack)
		sleep(2);

	if (do_pull) {
		printf("sleeping with socket open for pull\n");
		sleep(99999);
	}

	return (0);
}
