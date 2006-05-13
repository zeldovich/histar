extern "C" {
#include <inc/debug.h>
#include <inc/container.h>
#include <inc/error.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
}

#include <inc/scopeguard.hh>
#include <inc/dis/segserver.hh>
#include <inc/dis/segclient.hh>
#include <inc/dis/globallabel.hh>
#include <inc/dis/exportd.hh>  // for seg_stat
#include <inc/error.hh>

static const char conn_debug = 1;
static const char msg_debug = 1;
static const char file_debug = 1;

class open_req : public segserver_req 
{
public:
    open_req(segserver_hdr *header) : segserver_req(header) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "path %s",request_.path);
        executed_ = 1;

        global_label *gl = global_label::global_for_obj(request_.path);
        const char *s = gl->serial();
        int len = gl->serial_len();
        memcpy(response_.payload_, s, len);
        response_.header_.op = segclient_result;
        response_.header_.status = len;
        response_.header_.psize = len;
    }
};

class read_req : public segserver_req 
{
public:
    read_req(segserver_hdr *header) : segserver_req(header) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "count %d off %d path %s", 
                    request_.count, request_.offset, request_.path);
        executed_ = 1;

        int fd = open(request_.path, O_RDONLY);
        if (fd < 0) {
            debug_print(file_debug, "unable to open %s", request_.path);
            response_.header_.op = segclient_result;
            response_.header_.status = -1;
            return ;
        }
        scope_guard<int, int> close_fd(close, fd);
        int r = lseek(fd, request_.offset, SEEK_SET);
        if (r != (int64_t)request_.offset) {
            debug_print(file_debug, "lseek error %d, %d", r, request_.offset);
            response_.header_.op = segclient_result;
            response_.header_.status = -1;
            return ;
        }
        
        int cc = MIN(request_.count, sizeof(response_.payload_));
        int len = read(fd, response_.payload_, cc);
        response_.header_.op = segclient_result;
        response_.header_.status = len;
        response_.header_.psize = len;
        debug_print(file_debug, "read %d bytes from %s", len, request_.path);
    }
};

class write_req : public segserver_req 
{
public:
    write_req(int socket, segserver_hdr *header) : 
        segserver_req(header), socket_(socket) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "count %d off %d path %s", 
            request_.count, request_.offset, request_.path);
        executed_ = 1;

        // use payload as temp buffer
        char *buffer = response_.payload_;
        int cc = MIN(request_.count, sizeof(response_.payload_));

        int len = read(socket_, buffer, cc);
        if (len != cc)
            debug_print(file_debug, "truncated payload %d, %d", len, cc);

        int fd = open(request_.path, O_WRONLY);
        if (fd < 0) {
            debug_print(file_debug, "unable to open %s", request_.path);
            response_.header_.op = segclient_result;
            response_.header_.status = -1;
            return ;
        }
        scope_guard<int, int> close_fd(close, fd);
        int r = lseek(fd, request_.offset, SEEK_SET);
        if (r != (int64_t)request_.offset) {
            debug_print(file_debug, "lseek error %d, %d", r, request_.offset);
            response_.header_.op = segclient_result;
            response_.header_.status = -1;
            return ;   
        }

        len = write(fd, buffer, len);
    
        response_.header_.op = segclient_result;
        response_.header_.status = len;
        response_.header_.psize = 0;
        debug_print(file_debug, "wrote %d bytes to %s", len, request_.path);
    }
private:
    int socket_;
};

class stat_req : public segserver_req 
{
public:
    stat_req(segserver_hdr *header) : segserver_req(header) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "path %s", request_.path);
        executed_ = 1;
    
        int fd = open(request_.path, O_RDONLY);
        if (fd < 0) {
            debug_print(file_debug, "unable to open %s", request_.path);
            response_.header_.op = segclient_result;
            response_.header_.status = -1;
            return ;
        }
        scope_guard<int, int> close_fd(close, fd);
        
        struct stat st;
        int r = fstat(fd, &st);
        if (r < 0) {
            debug_print(file_debug, "stat error");
            response_.header_.op = segclient_result;
            response_.header_.status = r;
            return ;
        }
        struct seg_stat ss;
        ss.ss_size = st.st_size;
       
        int cc = MIN(sizeof(ss), sizeof(response_.payload_));
        memcpy(response_.payload_, &ss, cc);
        response_.header_.op = segclient_result;
        response_.header_.status = 0;
        response_.header_.psize = cc;
        debug_print(file_debug, "stat success");
    }
};

segserver_req *
segserver_conn::next_request(void) 
{
    segserver_hdr header;
    int r = read(socket_, &header, sizeof(header));
    if (r < 0)
        throw error(-E_INVAL, "socket read errno %d\n", errno);
    else if (r == 0)
        return 0;  // other end closed
    
    switch (header.op) {
        case segserver_open:
            return new open_req(&header);
        case segserver_read:
            return new read_req(&header);
        case segserver_write:
            return new write_req(socket_, &header);
        case segserver_stat:
            return new stat_req(&header);
        default:
            throw error(-E_INVAL, "unreconized op %d\n", header.op);
    }
    return 0;    
}

void 
segserver_conn::next_response_is(const segclient_msg *response) 
{
    // XXX
    int cc = sizeof(response->header_);
    int len = write(socket_, &response->header_, cc);     
    if (len < cc) {
        perror("error");
        throw error(-E_UNSPEC, "write error %d, %d", len, cc);
    }
    
    len = write(socket_, response->payload_, response->header_.psize);
    if (len < (int64_t) response->header_.psize)
        debug_print(conn_debug, "truncated write %d, %d", 
                    len, response->header_.psize);
}
