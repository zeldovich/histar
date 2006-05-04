extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>        
extern int poople(int domain, int type, int protocol);
}

int
fileclient_addr(char *host, int port, struct sockaddr_in *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    uint32_t ip;
    if ((ip = inet_addr(host)) != INADDR_NONE)
        addr->sin_addr.s_addr = ip;
    else {  
        struct hostent *hostent;
        if ((hostent = gethostbyname(host)) == 0) {
            printf("fileclient_addr: unable to resolve %s\n", host);
            return  -1;     
        }
        memcpy(&addr->sin_addr, hostent->h_addr, hostent->h_length) ;
    }
    return 0;    
}

int
fileclient_socket(void)
{
    return socket(AF_INET, SOCK_STREAM, 0);    
}

int
fileclient_connect(int s, struct sockaddr_in *addr)
{
    return connect(s, (struct sockaddr *)addr, sizeof(*addr));
}
