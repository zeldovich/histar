#include <inc/array.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <udev/jnic.h>
#include <udev/ne2kpci.h>

#include <string.h>
#include <malloc.h>

static int knic_init(struct cobj_ref obj, void** arg);
static int knic_macaddr(void *arg, uint8_t* macaddr);
static int knic_buf(void *arg, struct cobj_ref seg,
		    uint64_t offset, netbuf_type type);
static int64_t knic_wait(void *arg, uint64_t waiter_id, int64_t waitgen);

struct jnic_device {
    char type[16];
    int (*init)(struct cobj_ref obj, void** arg);
    int	(*net_macaddr)(void *arg, uint8_t* macaddr);
    int	(*net_buf)(void *arg, struct cobj_ref seg,
		   uint64_t offset, netbuf_type type);
    int64_t (*net_wait)(void *arg, uint64_t waiter_id,
			int64_t waitgen);
} devices[] = {
    { "kernel", knic_init, knic_macaddr, knic_buf, knic_wait },
    { "ne2kpci", ne2kpci_init, ne2kpci_macaddr, ne2kpci_buf, ne2kpci_wait },
    { "fxp", 0, 0, 0, 0 },
};

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

int
jnic_net_macaddr(struct jnic* jnic, uint8_t* macaddr)
{
    return devices[jnic->idx].net_macaddr(jnic->arg, macaddr);
}

int
jnic_net_buf(struct jnic* jnic, struct cobj_ref seg,
	     uint64_t offset, netbuf_type type)
{
    return devices[jnic->idx].net_buf(jnic->arg, seg, offset, type);    
}

int64_t
jnic_net_wait(struct jnic* jnic, uint64_t waiter_id, int64_t waitgen)
{
    return devices[jnic->idx].net_wait(jnic->arg, waiter_id, waitgen);        
}

int
jnic_init(struct jnic* jnic, struct cobj_ref obj, const char *type)
{
    int r;
    for (uint32_t i = 0; i < array_size(devices); i++) {
	if (!strcmp(type, devices[i].type)) {
	    if (!(r = devices[i].init(obj, &jnic->arg)))
		jnic->idx = i;
	    return r;
	}
    }
    return -E_INVAL;
}
