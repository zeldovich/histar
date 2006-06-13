extern "C" {
#include <inc/debug.h>
#include <inc/container.h>
#include <inc/error.h>
#include <inc/fs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <inc/syscall.h>

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
#include <inc/dis/exportc.hh>
#include <inc/dis/catc.hh>

#include <inc/error.hh>
#include <inc/labelutil.hh>

static const char conn_debug = 1;
static const char msg_debug = 1;
static const char file_debug = 1;

static int
package(const char *path, cobj_ref *ret)
{
    label th_l;
    thread_cur_label(&th_l);
    struct fs_inode ino;
    error_check(fs_namei(path, &ino));
    
    label f_l;
    obj_get_label(ino.obj, &f_l);
    
    if (f_l.compare(&th_l, label_leq_starhi) < 0) {
        catc cc;
        cobj_ref seg = cc.package(path);
        *ret = seg;
        return 1;
    }
    else {
        *ret = ino.obj;
        return 0;
    }
}

class open_req : public segserver_req 
{
public:
    open_req(segserver_hdr *header) : segserver_req(header) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "path %s",request_.path);
        executed_ = 1;

        export_managerc manager;
        export_segmentc seg = manager.segment_new(request_.path);
        conn_->export_seg_is(seg);
        
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

        try {
            struct fs_inode ino;
            int cc = MIN(request_.count, sizeof(response_.payload_));
            int len;
           
            cobj_ref seg;
            if (package(request_.path, &seg)) {
                ino.obj = seg;
                error_check(len = fs_pread(ino, response_.payload_, cc, request_.offset));
                sys_obj_unref(seg);
            }
            else {
                ino.obj = seg;
                error_check(len = fs_pread(ino, response_.payload_, cc, request_.offset));
            }
            response_.header_.op = segclient_result;
            response_.header_.status = 0;
            response_.header_.psize = len;
            debug_print(file_debug, "read %d bytes from %s", len, request_.path);
        } catch (basic_exception e) {
            response_.header_.op = segclient_result;
            response_.header_.status = -1;                
            response_.header_.psize = 0;
            printf("read_req failed: %s\n", e.what());
        }
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

        // read write-data from socket
        char *buffer = response_.payload_;
        int cc = MIN(request_.count, sizeof(response_.payload_));
        int len = read(socket_, buffer, cc);
        if (len != cc)
            debug_print(file_debug, "truncated payload %d, %d", len, cc);

        try {
            struct fs_inode ino;
            error_check(fs_namei(request_.path, &ino));    
            error_check(len = fs_pwrite(ino, buffer, len, request_.offset));
            response_.header_.op = segclient_result;
            response_.header_.status = len;
            response_.header_.psize = 0;
            debug_print(file_debug, "wrote %d bytes to %s", len, request_.path);
        } catch (basic_exception e) {
            response_.header_.op = segclient_result;
            response_.header_.psize = 0;
            response_.header_.status = -1;
            printf("write_req failed: %s", e.what());
        }
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

        try {
            struct fs_inode ino;
            error_check(fs_namei(request_.path, &ino));    
            uint64_t size;
            error_check(fs_getsize(ino, &size));
            
            seg_stat *ss = (seg_stat *)response_.payload_;
            ss->ss_size = size;
            
            response_.header_.op = segclient_result;
            response_.header_.status = 0;
            response_.header_.psize = sizeof(*ss);
            debug_print(file_debug, "stat success");
        } catch (basic_exception e) {
            response_.header_.op = segclient_result;
            response_.header_.psize = 0;
            response_.header_.status = -1;
            printf("stat_req failed: %s", e.what());
        }
    }
};

class auth_user_req : public segserver_req 
{
public:
    auth_user_req(segserver_conn *conn, segserver_hdr *header)
        : segserver_req(header), conn_(conn) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "auth req");
        executed_ = 1;
    
        char username[16];
        int cc = MIN(request_.count, sizeof(username));
        int len = read(conn_->socket(), username, cc);
        if (len != cc)
            debug_print(file_debug, "truncated payload %d, %d", len, cc);
    
        char *buffer = response_.payload_;
        int size = conn_->challenge_for(username, buffer);
        response_.header_.op = segclient_result;
        response_.header_.status = 0;
        response_.header_.psize = size;
        debug_print(file_debug, "auth req success");
    }
private:
    segserver_conn *conn_;
};

class auth_user_res : public segserver_req 
{
public:
    auth_user_res(segserver_conn *conn, segserver_hdr *header)
        : segserver_req(header), conn_(conn) {}
    virtual void execute(void) {
        if (executed_)
            throw error(-E_UNSPEC, "already executed");
        debug_print(msg_debug, "auth res");
        executed_ = 1;
    
        auth_msg payload;
        int cc = MIN(request_.count, sizeof(payload));
        int len = read(conn_->socket(), &payload, cc);
        if (len != cc)
            debug_print(file_debug, "truncated payload %d, %d", len, cc);
    
        conn_->response_for_is(payload.username, payload.subject, 
                               payload.subject_len);
        response_.header_.op = segclient_result;
        response_.header_.status = 0;
        response_.header_.psize = 0;
        debug_print(file_debug, "auth res success");
    }
private:
    segserver_conn *conn_;
};

int
segserver_conn::challenge_for(char *un, void *buf)
{ 
    // XXX
    return 1;    
}

void
segserver_conn::response_for_is(char *un, void *response, int n)
{ 
    // XXX
    return;    
}

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
        case segserver_authu_req:
            return new auth_user_req(this, &header);
        case segserver_authu_res:
            return new auth_user_res(this, &header);
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
