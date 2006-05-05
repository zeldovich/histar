extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>

#include <unistd.h>   
#include <fcntl.h>
#include <inc/debug.h>

}

#include <lib/dis/fileserver.hh>
#include <lib/dis/fileclient.hh>

static const char conn_debug = 1;
static const char msg_debug = 1;
static const char file_debug = 1;

char buffer[4096];

static int
fileserver_read_msg(int s, void *buf, uint64_t n)
{
    if (n < sizeof(fileserver_msg)) {
        printf("fileserver_read_msg: buf to small %ld\n", n);
        return 0;
    }
    int r = read(s, buf, sizeof(fileserver_msg));
    if (r != sizeof(fileserver_msg))
        printf("fileserver_read_msg: cheap-o size mismatch, %d %ld\n", 
                r, sizeof(fileserver_msg));
    return r;    
}

static void
fileserver_handle_read(int s, fileserver_msg *msg)
{
    debug_print(msg_debug, "count %d off %d path %s", 
                msg->count, msg->offset, msg->path);

    int fd = open(msg->path, O_RDONLY);
    if (fd < 0) {
        debug_print(file_debug, "unable to open %s", msg->path);
        fileclient_msg res;
        res.op = fileclient_result;
        res.status = -1;
        write(s, &res, sizeof(res));
        return ;
    }
    
    char buf[128];
    fileclient_msg *res;
    res = (fileclient_msg *) &buf;
    int len = read(fd, res->payload, 128 - sizeof(*res));
    res->op = fileclient_result;
    res->status = len;
    res->len = len;
    debug_print(file_debug, "read %d bytes from %s", len, msg->path);
    write(s, res, sizeof(*res) + len);
}

static void
fileserver_handle_msg(int s, void *buf, int len)
{
    fileserver_msg *msg = (fileserver_msg *) buf;
    fileclient_msg res;
    
    switch(msg->op) {
        case fileserver_read:
            fileserver_handle_read(s, msg);
            break;
        case fileserver_write:
            debug_print(conn_debug, "write request");
        default:
            res.op = fileclient_result;
            res.status = -1;
            write(s, &res, sizeof(res));
    }
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
        int len = fileserver_read_msg(ss, buffer, sizeof(buffer));
        debug_print(conn_debug, "incoming msg len %d", len);
            
        if (len)
            fileserver_handle_msg(ss, buffer, len);
        close(ss);
    }
}
