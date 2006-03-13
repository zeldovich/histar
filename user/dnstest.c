#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int
main(int ac, char **av)
{
    struct hostent *hp = gethostbyname("suif.stanford.edu");
    if (hp == 0) {
	herror("gethostbyname");
    } else {
	struct in_addr addr;
	memcpy(&addr, hp->h_addr, sizeof(addr));
	printf("ip addr %s\n", inet_ntoa(addr));
    }
}
