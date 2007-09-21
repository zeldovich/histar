extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   
}

#include <lib/dis/fileserver.hh>

extern "C" {
#include <netinet/in.h>
#include <stdio.h>    
#include <string.h>
#include <assert.h>
}

#include <lib/dis/globallabel.hh>
#include <lib/dis/proxydclnt.hh>
#include <lib/dis/proxydsrv.hh>

#include <inc/error.hh>

global_label::global_label(const char *path) : serial_(0), string_(0)
{
    static const uint32_t  bufsize = 256;
    char buf[bufsize];

    ((uint32_t*)buf)[0] = htonl(0);
    buf[4] = 1;
    
    serial_ = new char[5];
    memcpy(serial_, buf, 5);
    serial_len_ = 5;
}

const char *
global_label::serial(void) const 
{
    return serial_;
}
    
int         
global_label::serial_len(void) const
{
    return serial_len_;    
}


global_label *fileserver_new_global(char *a) { return new global_label(a); }
void fileserver_acquire(char *a, int b) { }

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
