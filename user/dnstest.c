#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

static void 
lookup(void)
{
    struct hostent *hp = gethostbyname("suif.stanford.edu");
    if (hp == 0) {
        herror("gethostbyname error");
    } else {
        struct in_addr addr;
        memcpy(&addr, hp->h_addr, sizeof(addr));
        printf("ip addr %s\n", inet_ntoa(addr));
    }
}

int
main(int ac, char **av)
{
    if (fork() == 0)
        lookup() ;
}
