#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/debug.h>
#include <inc/multisync.h>
#include <inc/setjmp.h>
#include <machine/memlayout.h>

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
	memset(&wstat[wcount], 0, sizeof(wstat[wcount]));
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
	memset(&wstat[wcount], 0, sizeof(wstat[wcount]));
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
    int64_t r;
    struct jcomm_ref data = ss->conn.data_comm;
    
    switch(a->op_type) {
    case jos64_op_accept: {
	r = jcomm_write(data, &a->accept, sizeof(a->accept), 1);
	if (r < 0)
	    return r;
	/* should never fail since sizeof(s->accept) < PIPE_BUF */
	assert(r == sizeof(a->accept));
	return 0;
    }
    case jos64_op_recv: {
	if (ss->dgram) {
	    r = jcomm_write(data, &a->recv.from,
			    sizeof(a->recv.from) + a->recv.cnt, 1);
	    if (r < 0) {
		debug_print(1, "(j%ld) %d byte dgram too big, dropping",
			    thread_id(), a->recv.cnt);
	    } else {
		assert(r == sizeof(a->recv.from) + a->recv.cnt);
	    }
	} else {
	    r = jcomm_write(data, &a->recv.buf[a->recv.off], a->recv.cnt, 1);
	    if (r < 0)
		return r;
	    if (r != a->recv.cnt) {
		a->recv.off += r;
		a->recv.cnt -= r;
		return -E_AGAIN;
	    }
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

    struct cobj_ref base_as;
    int r = sys_self_get_as(&base_as);
    if (r < 0) {
	cprintf("jos64_socket_thread: cannot get AS\n");
	return;
    }

    int64_t new_asid = sys_as_copy(base_as, start_env->proc_container,
				   0, "jos64_socket_thread as");
    if (new_asid < 0) {
	cprintf("jos64_socket_thread: cannot copy AS\n");
	return;
    }

    sys_self_set_as(COBJ(start_env->proc_container, new_asid));
    segment_as_switched();

    r = sys_segment_resize(COBJ(0, kobject_id_thread_sg), 2 * PGSIZE);
    if (r < 0) {
	debug_print(1, "(j%ld) cannot resize TLS: %s", thread_id(), e2s(r));
	return;
    }

    struct sock_slot *ss;
    if (sc->sock_id != -1) {
	ss = slot_from_id(sc->sock_id);
	if (!ss->used) {
	    status = -E_INVAL;
	    z = jcomm_write(sc->ctrl_comm, &status, sizeof(status), 1);
	    debug_print(z < 0, "(j%ld) error writing status: %s",
			thread_id(), e2s(z));
	    goto as_out;
	}
    } else {
	ss = slot_alloc();
	if (ss == 0) {
	    debug_print(1, "(j%ld) out of slots\n", thread_id());
	    status = -E_NO_MEM;
	    z = jcomm_write(sc->ctrl_comm, &status, sizeof(status), 1);
	    debug_print(z < 0, "(j%ld) error writing status: %s",
			thread_id(), e2s(z));
	    goto as_out;
	}

	ss->dgram = sc->dgram;
	ss->linuxthread_needed = 1;
	lutrap_kill(SIGNAL_NETD);
	while (!ss->linuxpid)
	    sys_sync_wait(&ss->linuxpid, 0, UINT64(~0));
    }

    /* register socket connection */
    ss->conn = *sc;
    struct jcomm_ref ctrl = ss->conn.ctrl_comm;
    struct jcomm_ref data = ss->conn.data_comm;

    struct jos_jmp_buf pgfault;
    if (jos_setjmp(&pgfault) != 0) {
	debug_print(1, "(j%ld) hit pagefault, shutting down", thread_id());
	goto done;
    }
    tls_data->tls_pgfault_all = &pgfault;

    debug_print(dbg, "(j%ld) and (l%ld)", thread_id(), ss->linuxpid);

    /* let our caller know we are clear */
    status = 0;
    z = jcomm_write(ctrl, &status, sizeof(status), 1);
    debug_print(z < 0, "(j%ld) error writing status: %s", thread_id(), e2s(z));

    /* wait for data on the jcomm, read into buffer shared with
     * the linux thread, and send SIGNAL_NETD to the jos64
     * linux thread.  The jos64 linux thread interrupts the
     * appropriate linux threads in netd_user_interrupt.  
     */
    for (;;) {
	r = jos64_wait_for(ss);
	if (r == 1) {
	    assert(ss->lnx2jos_full);
	    r = jos64_dispatch(ss, &ss->lnx2jos_buf);
	    if (r == -E_AGAIN)
		continue;
	    if (r < 0) {
		debug_print(1, "(j%ld) jos64_dispatch error: %s",
			    thread_id(), e2s(r));
		break;
	    }

	    ss->lnx2jos_full = CNT_LIMBO;
	    lutrap_kill(SIGNAL_NETD);
	} else if (r == 2) {
	    z = jcomm_read(ctrl, (void *)&ss->jos2lnx_buf, sizeof(ss->jos2lnx_buf), 1);
	    if (z < 0) {
		debug_print(dbg, "(j%ld) jcomm_read ctrl error: %s",
			    thread_id(), e2s(r));
		if (z == -E_NO_SPACE)
		    debug_print(1, "(j%ld) jcomm_read ctrl: no space", thread_id());
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
	    z = jcomm_write(ctrl, (void *)&ss->jos2lnx_buf, ss->jos2lnx_buf.size, 1);
	    assert(z == ss->jos2lnx_buf.size);
	} else if (r == 3) {
	    z = jcomm_read(data, (void *) &ss->outbuf[ss->outcnt],
			   sizeof(ss->outbuf) - ss->outcnt, 1);
	    if (z < 0) {
		debug_print(dbg, "(j%ld) jcomm_read data error: %s",
			    thread_id(), e2s(r));
		if (z == -E_NO_SPACE) {
		    debug_print(1, "(j%ld) jcomm_read data: no space", thread_id());
		    continue;
		} else {
		    break;
		}
	    }
	    if (z == 0) {
		debug_print(dbg, "(j%ld) jcomm_read data eof", thread_id());
		break;
	    }
	    ss->outcnt += z;
	    lutrap_kill(SIGNAL_NETD);
	} else {
	    debug_print(dbg, "(j%ld) r=%d", thread_id(), r);
	    break;
	}
    }

    /* Flush write data */
    for (;;) {
	while (ss->outcnt == sizeof(ss->outbuf))
	    sys_sync_wait(&ss->outcnt, sizeof(ss->outbuf), UINT64(~0));

	z = jcomm_read(data, (void *) &ss->outbuf[ss->outcnt],
		       sizeof(ss->outbuf) - ss->outcnt, 1);
	if (z == -E_NO_SPACE) {
	    debug_print(1, "(j%ld) jcomm_read data flush: no space", thread_id());
	    continue;
	}

	if (z <= 0)
	    break;

	ss->outcnt += z;
	lutrap_kill(SIGNAL_NETD);
    }


 done:
    ss->jos2lnx_buf.op_type = netd_op_close;
    ss->jos2lnx_full = 1;
    lutrap_kill(SIGNAL_NETD);
    debug_print(dbg, "(j%ld) stopping", thread_id());

 as_out:
    segment_as_invalidate_nowb(new_asid);
    sys_self_set_as(base_as);
    sys_obj_unref(COBJ(start_env->proc_container, new_asid));
}
