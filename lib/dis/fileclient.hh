#ifndef JOS_INC_FILECLIENT_HH_
#define JOS_INC_FILECLIENT_HH_

#include <arpa/inet.h>
#include <string.h>
#include <lib/dis/filemessage.h>
#include <unistd.h>

class global_label;

class file_frame
{
public:
    file_frame() {
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


class fileclient 
{
public:
    fileclient(char *path, char *host, int port) { 
        init(path, host, port);
    }
    ~fileclient(void) {
        destroy();    
    }
    void init(char *path, char *host, int port);
    void destroy(void); 
    
    // read, write
    const file_frame *frame_at(uint64_t count, uint64_t off);
    const file_frame *frame_at_is(void *va, uint64_t count, uint64_t off);

    const file_frame *frame(void) { return &frame_; }
    
    int stat(struct file_stat *buf);             
    
    const char *path(void) const { return path_; }

private:
    sockaddr_in     addr_;
    file_frame      frame_;
    global_label*   label_;
    char            path_[64];
    int             socket_;

};


#endif /*JOS_INC_FILECLIENT_HH_*/
