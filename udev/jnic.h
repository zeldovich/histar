#ifndef JOS_INC_JNIC_H
#define JOS_INC_JNIC_H

#include <inc/container.h>
#include <inc/netdev.h>

struct jnic
{
    void*    arg;
    uint32_t idx;
};

int	jnic_init(struct jnic* jnic, struct cobj_ref obj, const char *type);
int	jnic_net_macaddr(struct jnic* jnic, uint8_t* macaddr);
int	jnic_net_buf(struct jnic* jnic, struct cobj_ref seg,
		     uint64_t offset, netbuf_type type);
int64_t jnic_net_wait(struct jnic* jnic, uint64_t waiter_id, int64_t waitgen);

#endif
