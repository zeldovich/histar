#ifndef JOS_INC_FILEMESSAGE_H_
#define JOS_INC_FILEMESSAGE_H_

////////////////
// seg client
////////////////

typedef enum {
    segclient_result,
} segclient_msg_t;

struct segclient_hdr {
    segclient_msg_t op;
    int64_t status;
    uint32_t psize;
    char payload[0];
} __attribute__((packed));

struct segclient_msg {
    struct segclient_hdr header_;    
    char payload_[2000];
};

////////////////
// seg server
////////////////

typedef enum {
    segserver_open,
    segserver_read,
    segserver_write,
    segserver_stat,
} segserver_op_t;

struct segserver_hdr {
    segserver_op_t op;
    uint32_t count;
    uint32_t offset;
    char path[64];        
    char payload[0];
} __attribute__((packed));

struct segserver_msg {
   struct segserver_hdr header;
   char payload[2000];        
};

#endif /*JOS_INC_FILEMESSAGE_H_*/
