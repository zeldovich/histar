#ifndef JOS_INC_NETDEV_H
#define JOS_INC_NETDEV_H

#include <machine/types.h>

typedef enum {
    netbuf_rx,
    netbuf_tx
} netbuf_type;

#define NETHDR_COUNT_DONE	0x8000
#define NETHDR_COUNT_ERR	0x4000
#define NETHDR_COUNT_MASK	0x0fff

struct netbuf_hdr {
    uint16_t size;
    uint16_t actual_count;
};

#endif
