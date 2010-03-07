#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int
main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s desthost destport pktsize\n",
		    argv[0]);
		exit(1);
	}

	char *desthost = argv[1];
	uint16_t destport = atoi(argv[2]);
	unsigned int pktsize = atoi(argv[3]);

	int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in dst;

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port = htons(destport);
	inet_aton(desthost, &dst.sin_addr);

	printf("Sending packet of length %d to %s:%d\n", pktsize, desthost,
	    destport); 

	char *buf = malloc(pktsize);
	memset(buf, 0x55, pktsize);

	sendto(s, buf, pktsize, 0, (struct sockaddr *)&dst, sizeof(dst));

	return (0);
}
