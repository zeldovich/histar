#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <lwip/sockets.h>
#include <lwip/opt.h>
#include <api/ext.h>

#include <string.h>

extern struct lwip_socket *sockets;

int
lwipext_sync_waiting(int s, char w)
{
    if (s < 0 || s > NUM_SOCKETS)
	return -E_INVAL;

    if (w)
	sockets[s].send_wakeup = 1;
    else
	sockets[s].recv_wakeup = 1;

    return 0;
}

void
lwipext_sync_notify(int s, enum netconn_evt evt)
{
    if (evt == NETCONN_EVT_RCVPLUS) {
	if (sockets[s].rcvevent && sockets[s].recv_wakeup)
	    sys_sync_wakeup(&sockets[s].rcvevent);
    } else if (evt == NETCONN_EVT_SENDPLUS) {
	if (sockets[s].sendevent && sockets[s].send_wakeup)
	    sys_sync_wakeup(&sockets[s].sendevent);
    }
}

void
lwipext_init(char public_sockets)
{
    uint64_t bytes = NUM_SOCKETS * sizeof(struct lwip_socket);
    sockets = 0;
    struct cobj_ref seg;
    
    static const uint64_t nents = 2;
    uint64_t ents[nents];
    struct ulabel label =
	{ .ul_size = nents, .ul_ent = ents, .ul_nent = 0, .ul_default = 1 };

    int64_t r;
    assert(0 == label_set_level(&label, start_env->process_grant, 0, 0));
    if (!public_sockets)
	assert(0 == label_set_level(&label, start_env->process_taint, 3, 0));

    if ((r = segment_alloc(start_env->shared_container, bytes, &seg, 
			   (void *)&sockets, &label, "sockets")) < 0)
	panic("unable to alloc status seg: %s\n", e2s(r));
}
