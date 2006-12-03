#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <lwip/sockets.h>
#include <lwip/opt.h>
#include <api/jos64api.h>

#include <string.h>

extern struct lwip_socket *sockets;

static char read_notify[NUM_SOCKETS];
static char write_notify[NUM_SOCKETS];

int
jos64_sync_helper(int s, char write)
{
    if (s < 0 || s > NUM_SOCKETS)
	return -E_INVAL;
    
    if (write)
	write_notify[s] = 1;
    else
	read_notify[s] = 1;

    return 0;
}

void
jos64_event_helper(int s, enum netconn_evt evt)
{
    if (evt == NETCONN_EVT_RCVPLUS || evt == NETCONN_EVT_RCVMINUS) {
	if (sockets[s].rcvevent && read_notify[s]) {
	    read_notify[s] = 0;
	    sys_sync_wakeup(&sockets[s].rcvevent);
	}
    }  else {
	if (sockets[s].sendevent && write_notify[s]) {
	    write_notify[s] = 0;
	    sys_sync_wakeup(&sockets[s].sendevent);
	}
    }
}

void
jos64_init_api(void)
{
    memset(read_notify, 0, sizeof(read_notify));
    memset(write_notify, 0, sizeof(write_notify));
    
    uint64_t bytes = NUM_SOCKETS * sizeof(struct lwip_socket);
    sockets = 0;
    struct cobj_ref seg;
    
    static const uint64_t nents = 1;
    uint64_t ents[nents];
    struct ulabel label =
	{ .ul_size = nents, .ul_ent = ents };
    label.ul_default = 1;
    
    int64_t r = label_set_level(&label, start_env->process_grant, 0, 0);
    if (r < 0)
	panic("unable to set label level: %s\n", e2s(r));
    
    if ((r = segment_alloc(start_env->shared_container, bytes, &seg, 
			   (void *)&sockets, &label, "sockets")) < 0)
	panic("unable to alloc status seg: %s\n", e2s(r));
}
