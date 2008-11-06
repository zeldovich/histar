#ifndef JOS_INC_JNIC_H
#define JOS_INC_JNIC_H

#include <inc/container.h>
#include <inc/netdev.h>

/* 
 * lwIP interface 
 */
struct jnic
{
    void*    arg;
    uint32_t idx;	/* index into user-level devices array */
};

int	jnic_match(struct jnic* jnic, struct cobj_ref obj, uint64_t key);
int	jnic_net_macaddr(struct jnic* jnic, uint8_t* macaddr);
int	jnic_net_buf(struct jnic* jnic, struct cobj_ref seg,
		     uint64_t offset, netbuf_type type);
int64_t jnic_net_wait(struct jnic* jnic, uint64_t waiter_id, int64_t waitgen);


/*
 * device driver interface
 */
struct jnic_device	 
{
    int (*init)(struct cobj_ref obj, void** arg);
    int	(*net_macaddr)(void *arg, uint8_t* macaddr);
    int	(*net_buf)(void *arg, struct cobj_ref seg,
		   uint64_t offset, netbuf_type type);
    int64_t (*net_wait)(void *arg, uint64_t waiter_id,
			int64_t waitgen);
};


#endif
