extern "C" {
#include <inc/error.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>        
#include <errno.h>
#include <unistd.h>
#include <inc/debug.h>
}

#include <lib/dis/fileserver.hh>
#include <lib/dis/fileclient.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

static const char msg_debug = 1;

void
fileclient::init(char *path, char *host, int port)
{
    strcpy(path_, path);
    frame_.init();
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    uint32_t ip;
    if ((ip = inet_addr(host)) != INADDR_NONE)
        addr_.sin_addr.s_addr = ip;
    else {  
        struct hostent *hostent;
        if ((hostent = gethostbyname(host)) == 0)
            throw error(-E_INVAL, "unable to resolve %s", host);
        memcpy(&addr_.sin_addr, hostent->h_addr, hostent->h_length) ;
    }
}

const file_frame*
fileclient::frame_at(uint64_t count, uint64_t offset)
{
    error_check(socket_ = socket(AF_INET, SOCK_STREAM, 0));       
    error_check(connect(socket_, (struct sockaddr *)&addr_, sizeof(addr_)));
    scope_guard<int, int> close_socket(close, socket_);
    
    fileserver_msg msg;
    msg.op = fileserver_read;
    msg.count = MIN(count, frame_.bytes_);
    msg.offset = offset;
    strcpy(msg.path, path_);
    debug_print(msg_debug, "count %d off %d path %s", 
                msg.count, msg.offset, msg.path);
    
    // XXX
    error_check(write(socket_, &msg, sizeof(msg)) - sizeof(msg));

    fileclient_msg res;
    // XXX
    error_check(read(socket_, &res, sizeof(res)) - sizeof(res));
    error_check(read(socket_, frame_.byte_, res.len) - res.len);
    frame_.offset_ = offset;
    frame_.count_ = res.len;
            
    return &frame_;    
}
