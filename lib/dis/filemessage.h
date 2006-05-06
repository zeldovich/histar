#ifndef JOS_INC_FILEMESSAGE_H_
#define JOS_INC_FILEMESSAGE_H_

////////////////
// fileclient
////////////////

typedef enum {
    fileclient_result,
} fileclient_msg_t;

struct fileclient_hdr {
    fileclient_msg_t op;
    int64_t status;
    uint32_t psize;
    char payload[0];
} __attribute__((packed));

struct fileclient_msg {
    fileclient_hdr header_;    
    char payload_[2000];
};

////////////////
// fileserver
////////////////

typedef enum {
    fileserver_read,
    fileserver_write,
    fileserver_stat,
} fileserver_op_t;

struct fileserver_hdr {
    fileserver_op_t op;
    uint32_t count;
    uint32_t offset;
    char path[64];        
    char payload[0];
} __attribute__((packed));

struct fileserver_msg {
   fileserver_hdr header;
    char payload[2000];        
};

#endif /*JOS_INC_FILEMESSAGE_H_*/
