extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>

#include <unistd.h>   
}

#include <lib/dis/fileserver.hh>

char buf[4096];

void
fileserver_start(int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("cannot create socket: %d\n", s);
        return;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0) {
        printf("cannot bind socket: %d\n", r);
        return;
    }

    r = listen(s, 5);
    if (r < 0) {
        printf("cannot listen on socket: %d\n", r);
        return;
    }
    
    while (1) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            return;
        }
        // XXX ...
    }
}
