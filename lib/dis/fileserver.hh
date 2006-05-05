#ifndef JOS_INC_FILESERVER_HH_
#define JOS_INC_FILESERVER_HH_

#ifdef JOS_USER
#include <inc/types.h>
#else
#include <sys/types.h>
#endif // JOS_USER

void fileserver_start(int port);

enum {
    fileserver_read,
    fileserver_write,
};
typedef uint8_t fileserver_op_t;

struct fileserver_msg {
    fileserver_op_t op;
    uint32_t count;
    uint32_t offset;
    char path[64];        
    char payload[0];
} __attribute__((packed));

#endif /*JOS_INC_FILESERVER_HH_*/
