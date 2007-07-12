#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/debug.h>
#include <inc/multisync.h>
#include <inc/setjmp.h>

#include <stdio.h>
#include <string.h>

#include <os-jos64/lutrap.h>
#include <archcall.h>

#include "netduser.h"

enum { dbg = 0 };

/* returns 1 for linux socket event, 2 for a signal on the control comm, 
 * 3 for a signal on the data comm, 0 for unknown
 */
static int
jos64_wait_for(struct sock_slot *ss)
{
    struct wait_stat wstat[3];
    int x, y, z, r;
    
 top:
    x = jcomm_multisync(ss->conn.ctrl_comm, dev_probe_read, &wstat[0]);
    y = jcomm_multisync(ss->conn.data_comm, dev_probe_read, &wstat[1]);
    if (x < 0 || y < 0)
	panic("jcomm_mutlisync error: %s, %s", e2s(x), e2s(y));
    memset(&wstat[2], 0, sizeof(wstat[2]));
    WS_SETADDR(&wstat[2], &ss->josfull);
    WS_SETVAL(&wstat[2], ss->josfull);

    if (ss->josfull && ss->josfull != CNT_LIMBO) {
	z = jcomm_probe(ss->conn.data_comm, dev_probe_write);
	if (z)
	    return 1;
	z = jcomm_multisync(ss->conn.data_comm, dev_probe_write, &wstat[2]);
	if (z < 0)
	    panic("jcomm_mutlisync error: %s",  e2s(z));
    }
    
    x = jcomm_probe(ss->conn.ctrl_comm, dev_probe_read);
    if (x)
	return 2;
    y = jcomm_probe(ss->conn.data_comm, dev_probe_read);
    if (y)
	return 3;
    
    r = multisync_wait(wstat, 3, UINT64(~0));
    if (r < 0)
	panic("multisync_wait error: %s", e2s(r));
    
    if (ss->josfull && ss->josfull != CNT_LIMBO) {
	z = jcomm_probe(ss->conn.data_comm, dev_probe_write);
	if (z)
	    return 1;
    }

    x = jcomm_probe(ss->conn.ctrl_comm, dev_probe_read);
    if (x)
	return 2;
    y = jcomm_probe(ss->conn.data_comm, dev_probe_read);
    if (y)
	return 3;

    goto top;
}

static int
jos64_dispatch(struct sock_slot *ss, struct jos64_op_args *a)
{
    int r;
    struct jcomm_ref data = ss->conn.data_comm;
    
    switch(a->op_type) {
    case jos64_op_accept: {
	int64_t z = jcomm_write(data, &a->accept, sizeof(a->accept));
	if (z < 0)
	    return z;
	/* should never fail since sizeof(s->accept) < PIPE_BUF */
	assert(z == sizeof(a->accept));
	return 0;
    }
    case jos64_op_recv: {
	int64_t z = jcomm_write(data, &a->recv.buf[a->recv.off], a->recv.cnt);
	if (z < 0)
	    return z;
	if (z != ss->josbuf.recv.cnt) {
	    ss->josbuf.recv.off += z;
	    ss->josbuf.recv.cnt -= z;
	    return -E_AGAIN;
	}
	return 0;
    }
    case jos64_op_shutdown:
	r = jcomm_shut(data, JCOMM_SHUT_WR);
	return r;
    default:
	arch_printf("jos64_dispatch: unimplemented %d\n", a->op_type);
	return -E_BAD_OP;
    }
    return 0;
}

void
jos64_socket_thread(struct socket_conn *sc)
{
    int64_t z;
    int status ;
    debug_print(dbg, "(j%ld) starting", thread_id());
    struct sock_slot *ss;
    if (sc->sock_id != -1) {
	ss = slot_from_id(sc->sock_id);
	if (!ss->used) {
	    status = -E_INVAL;
	    z = jcomm_write(sc->ctrl_comm, &status, sizeof(status));
	    debug_print(z < 0, "error writing status: %s", e2s(z));
	    return;
	}
    } else {
	ss = slot_alloc();
	if (ss == 0)
	    panic("no slots");
	lutrap_kill(SIGNAL_NETD);
	sys_sync_wait(&ss->linuxpid, 0, UINT64(~0));
    }

    /* register socket connection */
    ss->conn = *sc;
    struct jcomm_ref ctrl = ss->conn.ctrl_comm;
    struct jcomm_ref data = ss->conn.data_comm;

    struct jos_jmp_buf pgfault;
    if (jos_setjmp(&pgfault) != 0)
	goto done;
    tls_data->tls_pgfault_all = &pgfault;

    debug_print(dbg, "(j%ld) and (l%ld)", thread_id(), ss->linuxpid);

    /* let our caller know we are clear */
    status = 0;
    z = jcomm_write(ctrl, &status, sizeof(status));
    debug_print(z < 0, "error writing status: %s", e2s(z));

    /* wait for data on the jcomm, read into buffer shared with
     * the linux thread, and send SIGNAL_NETD to the jos64
     * linux thread.  The jos64 linux thread interrupts the
     * appropriate linux threads in netd_user_interrupt.  
     */
    for (;;) {
	int r = jos64_wait_for(ss);
	if (r == 1) {
	    assert(ss->josfull);
	    r = jos64_dispatch(ss, &ss->josbuf);
	    if (r == -E_AGAIN)
		continue;
	    else if (r < 0)
		panic("jos64_dispatch error: %s\n", e2s(r));
	    ss->josfull = CNT_LIMBO;
	    lutrap_kill(SIGNAL_NETD);
	    sys_sync_wait(&ss->josfull, CNT_LIMBO, UINT64(~0));
	} else if (r == 2) {
	    int64_t z = jcomm_read(ctrl, (void *)&ss->opbuf, sizeof(ss->opbuf));
	    if (z < 0) {
		debug_print(dbg, "jcomm_read error: %s\n", e2s(r));
		break;
	    } else if (ss->opbuf.op_type == netd_op_close)
		break;
	    
	    ss->opfull = 1;
	    lutrap_kill(SIGNAL_NETD);
	    sys_sync_wait(&ss->opfull, 1, UINT64(~0));
	    
	    /* send return value */
	    z = jcomm_write(ctrl, (void *)&ss->opbuf, ss->opbuf.size);
	    assert(z == ss->opbuf.size);
	} else if (r == 3) {
	    /* XXX is there any reason not to read client data from the
	     * ctrl comm?
	     */
	    int64_t z = jcomm_read(data, (void *)&ss->outbuf, sizeof(ss->outbuf));
	    if (z < 0) {
		debug_print(dbg, "jcomm_read error: %s\n", e2s(r));
		break;
	    } 
	    ss->outcnt = z;
	    lutrap_kill(SIGNAL_NETD);
	    sys_sync_wait(&ss->outcnt, z, UINT64(~0));
	}
    }

 done:
    ss->opbuf.op_type = netd_op_close;
    ss->opfull = 1;
    lutrap_kill(SIGNAL_NETD);
    debug_print(dbg, "(j%ld) stopping", thread_id());
}