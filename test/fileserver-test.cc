extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   
}

#include <lib/dis/fileserver.hh>

static void 
usage(char *com)
{
    printf("usage: %s port\n", com);    
    exit(-1);
}

int
main (int ac, char **av)
{
    if (ac < 2)
        usage(av[0]);
    int port = atoi(av[1]);
    fileserver_start(port);
    return 0;    
}
