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
    struct wait_stat wstat[6];
    int r, wcount;

 top:
    wcount = 0;
    r = jcomm_multisync(ss->conn.ctrl_comm, dev_probe_read, &wstat[wcount], 2);
    if (r < 0)
	return r;
    wcount += r;

    r = jcomm_probe(ss->conn.ctrl_comm, dev_probe_read);
    if (r > 0)
	return 2;

    uint64_t outcnt = ss->outcnt;
    if (outcnt < sizeof(ss->outbuf) / 2) {
	r = jcomm_multisync(ss->conn.data_comm, dev_probe_read,
			    &wstat[wcount], 2);
	if (r < 0)
	    return r;
	wcount += r;

	r = jcomm_probe(ss->conn.data_comm, dev_probe_read);
	if (r > 0)
	    return 3;
    } else {
	/* Wait for buffer space to become available */
	WS_SETADDR(&wstat[wcount], &ss->outcnt);
	WS_SETVAL(&wstat[wcount], outcnt);
	wcount++;
    }

    uint64_t lnx2jos_full = ss->lnx2jos_full;
    if (lnx2jos_full && lnx2jos_full != CNT_LIMBO) {
	r = jcomm_multisync(ss->conn.data_comm, dev_probe_write,
			    &wstat[wcount], 2);
	if (r < 0)
	    return r;
	wcount += r;

	r = jcomm_probe(ss->conn.data_comm, dev_probe_write);
	if (r > 0)
	    return 1;
    } else {
	/* Wait for the buffer to get some data */
	WS_SETADDR(&wstat[wcount], &ss->lnx2jos_full);
	WS_SETVAL(&wstat[wcount], lnx2jos_full);
	wcount++;
    }

    debug_print(dbg, "(j%ld) waiting..", thread_id());
    r = multisync_wait(wstat, wcount, UINT64(~0));
    if (r < 0) {
	debug_print(dbg, "(j%ld) wait error: %s", thread_id(), e2s(r));
	return r;
    }
    debug_print(dbg, "(j%ld) woke up", thread_id());

    lnx2jos_full = ss->lnx2jos_full;
    if (lnx2jos_full && lnx2jos_full != CNT_LIMBO) {
	r = jcomm_probe(ss->conn.data_comm, dev_probe_write);
	if (r > 0)
	    return 1;
    }

    r = jcomm_probe(ss->conn.ctrl_comm, dev_probe_read);
    if (r > 0)
	return 2;

    if (ss->outcnt < sizeof(ss->outbuf) / 2) {
	r = jcomm_probe(ss->conn.data_comm, dev_probe_read);
	if (r > 0)
	    return 3;
    }

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
	if (z != ss->lnx2jos_buf.recv.cnt) {
	    ss->lnx2jos_buf.recv.off += z;
	    ss->lnx2jos_buf.recv.cnt -= z;
	    return -E_AGAIN;
	}
	return 0;
    }
    case jos64_op_shutdown:
	r = jcomm_shut(data, JCOMM_SHUT_WR);
	debug_print(dbg, "(j%ld) shutdown writes: %d", thread_id(), r);
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
	    debug_print(z < 0, "(j%ld) error writing status: %s",
			thread_id(), e2s(z));
	    return;
	}
    } else {
	ss = slot_alloc();
	if (ss == 0)
	    panic("no slots");
	lutrap_kill(SIGNAL_NETD);
	while (!ss->linuxpid)
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
    debug_print(z < 0, "(j%ld) error writing status: %s", thread_id(), e2s(z));

    /* wait for data on the jcomm, read into buffer shared with
     * the linux thread, and send SIGNAL_NETD to the jos64
     * linux thread.  The jos64 linux thread interrupts the
     * appropriate linux threads in netd_user_interrupt.  
     */
    for (;;) {
	int r = jos64_wait_for(ss);
	if (r == 1) {
	    assert(ss->lnx2jos_full);
	    r = jos64_dispatch(ss, &ss->lnx2jos_buf);
	    if (r == -E_AGAIN)
		continue;
	    if (r < 0)
		panic("jos64_dispatch error: %s\n", e2s(r));
	    ss->lnx2jos_full = CNT_LIMBO;
	    lutrap_kill(SIGNAL_NETD);
	} else if (r == 2) {
	    int64_t z = jcomm_read(ctrl, (void *)&ss->jos2lnx_buf, sizeof(ss->jos2lnx_buf));
	    if (z < 0) {
		debug_print(dbg, "(j%ld) jcomm_read ctrl error: %s",
			    thread_id(), e2s(r));
		break;
	    }

	    if (z == 0) {
		debug_print(dbg, "(j%ld) jcomm_read ctrl eof", thread_id());
		break;
	    }

	    if (ss->jos2lnx_buf.op_type == netd_op_close) {
		debug_print(dbg, "(j%ld) netd_op_close", thread_id());
		break;
	    }

	    ss->jos2lnx_full = 1;
	    lutrap_kill(SIGNAL_NETD);
	    while (ss->jos2lnx_full)
		sys_sync_wait(&ss->jos2lnx_full, 1, UINT64(~0));

	    /* send return value */
	    z = jcomm_write(ctrl, (void *)&ss->jos2lnx_buf, ss->jos2lnx_buf.size);
	    assert(z == ss->jos2lnx_buf.size);
	} else if (r == 3) {
	    int64_t z = jcomm_read(data, (void *) &ss->outbuf[ss->outcnt],
				   sizeof(ss->outbuf) - ss->outcnt);
	    if (z < 0) {
		debug_print(dbg, "(j%ld) jcomm_read data error: %s",
			    thread_id(), e2s(r));
		break;
	    }
	    if (z == 0) {
		debug_print(dbg, "(j%ld) jcomm_read data eof", thread_id());
		break;
	    }
	    ss->outcnt += z;
	    lutrap_kill(SIGNAL_NETD);
	} else {
	    break;
	}
    }

 done:
    ss->jos2lnx_buf.op_type = netd_op_close;
    ss->jos2lnx_full = 1;
    lutrap_kill(SIGNAL_NETD);
    debug_print(dbg, "(j%ld) stopping", thread_id());
}
