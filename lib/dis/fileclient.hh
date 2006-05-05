#ifndef JOS_INC_FILECLIENT_HH_
#define JOS_INC_FILECLIENT_HH_

#include <arpa/inet.h>
#include <string.h>

typedef enum {
    fileclient_result,
} fileclient_msg_t;

struct fileclient_msg {
    fileclient_msg_t op;
    int status;
    uint32_t len;
    char payload[0];
};

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
    uint64_t count_;
    
    static const uint64_t bytes_ = 2000;
    char byte_[bytes_];
};


class fileclient 
{
public:
    fileclient();
    void init(char *path, char *host, int port);
    
    const file_frame *frame_at(uint64_t count, uint64_t off);
    const file_frame *frame_at_is(void *va, uint64_t count, uint64_t off);
    
    const char *path(void) const { return path_; }

    sockaddr_in addr_;
private:
    file_frame  frame_;
    char        path_[64];
    int         socket_;

};


#endif /*JOS_INC_FILECLIENT_HH_*/
