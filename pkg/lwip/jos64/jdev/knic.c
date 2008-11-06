#include <inc/container.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <jdev/jnic.h>
#include <jdev/knic.h>

#include <malloc.h>

static int
knic_init(struct cobj_ref obj, void** arg)
{
    struct cobj_ref *argp = malloc(sizeof(obj));
    if (!argp)
	return -E_NO_MEM;
    *argp = obj;
    *arg = argp;
    return 0;
}

static int 
knic_macaddr(void *arg, uint8_t* macaddr)
{
    struct cobj_ref *o = arg;
    return sys_net_macaddr(*o, macaddr);
}

static int 
knic_buf(void *arg, struct cobj_ref seg,
	 uint64_t offset, netbuf_type type)
{
    struct cobj_ref *o = arg;
    return sys_net_buf(*o, seg, offset, type);
}

static int64_t 
knic_wait(void *arg, uint64_t waiter_id, int64_t waitgen)
{
    struct cobj_ref *o = arg;
    return sys_net_wait(*o, waiter_id, waitgen);
}

struct jnic_device knic_jnic = {
    .init	 = knic_init, 
    .net_macaddr = knic_macaddr, 
    .net_buf	 = knic_buf, 
    .net_wait	 = knic_wait,
};
