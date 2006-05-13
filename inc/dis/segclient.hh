#ifndef JOS_INC_FILECLIENT_HH_
#define JOS_INC_FILECLIENT_HH_

#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

class global_label;
struct seg_stat;

class seg_frame
{
public:
    seg_frame() {
        init();
    }
    void init() {
        offset_ = 0;
        count_ = 0;
        memset(byte_, 0, bytes_);
    }
    
    uint64_t offset_;
    int64_t count_;
    
    static const uint64_t bytes_ = 2000;
    char byte_[bytes_];
};


class seg_client 
{
public:
    seg_client(char *path, char *host, int port) { 
        init(path, host, port);
    }
    ~seg_client(void) {
        destroy();    
    }
    void init(char *path, char *host, int port);
    void destroy(void); 
    
    // read, write
    const seg_frame *frame_at(uint64_t count, uint64_t off);
    const seg_frame *frame_at_is(void *va, uint64_t count, uint64_t off);
    const seg_frame *frame(void) { return &frame_; }
    int stat(struct seg_stat *buf);             

    void auth_user_is(const char *un);
    
    const char *path(void) const { return path_; }

private:
    sockaddr_in     addr_;
    seg_frame      frame_;
    global_label*   label_;
    char            path_[64];
    int             socket_;

};


#endif /*JOS_INC_FILECLIENT_HH_*/
