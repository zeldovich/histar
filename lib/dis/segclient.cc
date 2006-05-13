extern "C" {
#include <inc/error.h>

#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>        
#include <errno.h>
#include <unistd.h>
#include <inc/debug.h>

#include <sys/param.h>
}

#include <inc/dis/segserver.hh>
#include <inc/dis/segclient.hh>
#include <inc/dis/globallabel.hh>
#include <inc/dis/exportclient.hh>
#include <inc/dis/exportd.hh>  // for seg_stat

#include <inc/error.hh>
#include <inc/scopeguard.hh>

static const char msg_debug = 0;

void
seg_client::init(char *path, char *host, int port)
{
    if (strlen(path) + 1 > sizeof(path_))
        throw error(-E_INVAL, "'%s' too long (> %ld)", path, sizeof(path_));
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
    
    error_check(socket_ = socket(AF_INET, SOCK_STREAM, 0));       
    error_check(connect(socket_, (struct sockaddr *)&addr_, sizeof(addr_)));
    segserver_hdr msg;
    msg.op = segserver_open;
    strcpy(msg.path, path_);
    // use frame as temp buffer
    char *buffer = frame_.byte_;
    try {
        // XXX
        error_check(write(socket_, &msg, sizeof(msg)) - sizeof(msg));
        segclient_hdr res;
        error_check(read(socket_, &res, sizeof(res)) - sizeof(res));
        error_check(res.status);
        error_check(read(socket_, buffer, res.psize) - res.psize);
    } catch (basic_exception e) {
        close(socket_);
        throw e;    
    }
    label_ = new global_label(buffer);
}

void
seg_client::destroy(void)
{
    close(socket_);    
    delete label_;
}

const seg_frame*
seg_client::frame_at(uint64_t count, uint64_t offset)
{
    segserver_hdr msg;
    msg.op = segserver_read;
    msg.count = MIN(count, frame_.bytes_);
    msg.offset = offset;
    strcpy(msg.path, path_);
    debug_print(msg_debug, "count %d off %d path %s", 
                msg.count, msg.offset, msg.path);
    
    // XXX
    error_check(write(socket_, &msg, sizeof(msg)) - sizeof(msg));

    segclient_hdr res;
    // XXX
    error_check(read(socket_, &res, sizeof(res)) - sizeof(res));
    error_check(read(socket_, frame_.byte_, res.psize) - res.psize);
    frame_.offset_ = offset;
    frame_.count_ = res.status;
            
    return &frame_;    
}

const seg_frame*
seg_client::frame_at_is(void *va, uint64_t count, uint64_t offset)
{
    int cc = MIN(count, frame_.bytes_);
    memcpy(frame_.byte_, va, cc);
    
    segserver_hdr msg;
    msg.op = segserver_write;
    msg.count = cc;
    msg.offset = offset;
    strcpy(msg.path, path_);
    debug_print(msg_debug, "count %d off %d path %s", 
                msg.count, msg.offset, msg.path);
    
    // XXX
    error_check(write(socket_, &msg, sizeof(msg)) - sizeof(msg));
    error_check(write(socket_, &frame_.byte_, cc) - cc);

    segclient_hdr res;
    // XXX
    error_check(read(socket_, &res, sizeof(res)) - sizeof(res));
    frame_.offset_ = offset;
    frame_.count_ = res.status;
            
    return &frame_;    
}

int 
seg_client::stat(struct seg_stat *buf)
{
    segserver_hdr msg;
    msg.op = segserver_stat;
    strcpy(msg.path, path_);
    debug_print(msg_debug, "path %s", msg.path);
    
    // XXX
    error_check(write(socket_, &msg, sizeof(msg)) - sizeof(msg));

    segclient_hdr res;
    // XXX
    error_check(read(socket_, &res, sizeof(res)) - sizeof(res));
    int cc = MIN(res.psize, sizeof(*buf));
    if (res.psize != sizeof(*buf))
        printf("unexpected stat len %d, %ld\n", res.psize, sizeof(*buf));
    // XXX
    error_check(read(socket_, buf, cc) - cc);    
    
    return res.status;    
}
