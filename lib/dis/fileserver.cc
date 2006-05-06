extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>

#include <fcntl.h>
#include <inc/debug.h>
#include <sys/param.h>

#include <inc/error.h>
}

#include <inc/scopeguard.hh>
#include <lib/dis/fileserver.hh>
#include <lib/dis/fileclient.hh>
#include <inc/error.hh>

static const char conn_debug = 1;
static const char msg_debug = 1;
static const char file_debug = 1;

class read_req : public fileserver_req 
{
public:
    read_req(fileserver_hdr *header) : fileserver_req(header) {}
    virtual void execute(void) {
        debug_print(msg_debug, "count %d off %d path %s", 
                    request_.count, request_.offset, request_.path);
        executed_ = 1;
    
        int fd = open(request_.path, O_RDONLY);
        scope_guard<int, int> close_fd(close, fd);
        if (fd < 0) {
            debug_print(file_debug, "unable to open %s", request_.path);
            response_.header_.op = fileclient_result;
            response_.header_.status = -1;
            return ;
        }
        int r = lseek(fd, request_.offset, SEEK_SET);
        if (r != (int64_t)request_.offset) {
            debug_print(file_debug, "lseek error %d, %d", r, request_.offset);
            response_.header_.op = fileclient_result;
            response_.header_.status = -1;
            return ;
        }
        
        int cc = MIN(request_.count, sizeof(response_.payload_));
        int len = read(fd, response_.payload_, cc);
        response_.header_.op = fileclient_result;
        response_.header_.status = len;
        response_.header_.psize = len;
        debug_print(file_debug, "read %d bytes from %s", len, request_.path);
    }
};

class write_req : public fileserver_req 
{
public:
    write_req(int socket, fileserver_hdr *header) : 
        fileserver_req(header), socket_(socket) {}
    virtual void execute(void) {
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
        scope_guard<int, int> close_fd(close, fd);
        if (fd < 0) {
            debug_print(file_debug, "unable to open %s", request_.path);
            response_.header_.op = fileclient_result;
            response_.header_.status = -1;
            return ;
        }
        int r = lseek(fd, request_.offset, SEEK_SET);
        if (r != (int64_t)request_.offset) {
            debug_print(file_debug, "lseek error %d, %d", r, request_.offset);
            response_.header_.op = fileclient_result;
            response_.header_.status = -1;
            return ;   
        }

        len = write(fd, buffer, len);
    
        response_.header_.op = fileclient_result;
        response_.header_.status = len;
        response_.header_.psize = 0;
        debug_print(file_debug, "wrote %d bytes to %s", len, request_.path);
    }
private:
    int socket_;
};

fileserver_req *
fileserver_conn::next_request(void) 
{
    fileserver_hdr header;
    // XXX
    error_check(read(socket_, &header, sizeof(header)) - sizeof(header));
    switch (header.op) {
        case fileserver_read:
            return new read_req(&header);
        case fileserver_write:
            return new write_req(socket_, &header);
        default:
            throw error(-E_INVAL, "unreconized op %d\n", header.op);
    }
    return 0;    
}

void 
fileserver_conn::next_response_is(const fileclient_msg *response) 
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
        
        debug_print(conn_debug, "waiting for connections");
        
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            return;
        }

        char *ip = (char *)&sin.sin_addr.s_addr;
        debug_print(conn_debug, "connection from %d.%d.%d.%d", 
               ip[0], ip[1], ip[2], ip[3]);

        // one shot            
        fileserver_conn *conn = new fileserver_conn(ss, sin);
        scope_guard<void, fileserver_conn *> del_conn(delete_obj, conn);
        fileserver_req *req = conn->next_request();
        scope_guard<void, fileserver_req *> del_req(delete_obj, req);
        req->execute();
        const fileclient_msg *resp = req->response();
        conn->next_response_is(resp);
    }
}
