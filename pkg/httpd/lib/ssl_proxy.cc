extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/debug.h>
#include <inc/assert.h>
#include <inc/fd.h>
#include <inc/bipipe.h>

#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
}

#include <inc/sslproxy.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/ssldclnt.hh>

static const char dbg = 0;

#define SELECT_SOCK    0x0001
#define SELECT_CIPHER  0x0002

void
ssl_proxy_alloc(cobj_ref ssld_gate, cobj_ref eproc_gate, 
		uint64_t base_ct, int sock_fd, ssl_proxy_descriptor *d)
{
    uint64_t ssl_taint = handle_alloc();
    scope_guard<void, uint64_t> drop_star(thread_drop_star, ssl_taint);
    label taint_label(1);
    taint_label.set(ssl_taint, 3);

    int64_t ssl_root_ct = sys_container_alloc(base_ct,
					      taint_label.to_ulabel(),
					      "ssl-root", 0, CT_QUOTA_INF);
    error_check(ssl_root_ct);
    d->base_ct_ = base_ct;
    d->ssl_ct_ = ssl_root_ct;
    d->sock_fd_ = sock_fd;

    try {
	// manually setup bipipe segments
	struct cobj_ref cipher_seg;
	error_check(bipipe_alloc(ssl_root_ct, &cipher_seg, 
				 taint_label.to_ulabel(), "cipher-bipipe"));
	
	struct cobj_ref plain_seg;
	error_check(bipipe_alloc(ssl_root_ct,&plain_seg, 
				 taint_label.to_ulabel(), "plain-bipipe"));
	
	struct cobj_ref eproc_seg = COBJ(0, 0);
	if (eproc_gate.object) {
	    error_check(bipipe_alloc(ssl_root_ct, &eproc_seg, 
				     taint_label.to_ulabel(), "eproc-bipipe"));
	}

	d->cipher_bipipe_ = cipher_seg;
	d->taint_ = ssl_taint;

	struct ssl_proxy_client *spc = 0;
	error_check(segment_alloc(ssl_root_ct, sizeof(*spc), &d->client_seg_, 
				  (void **)&spc, taint_label.to_ulabel(), 
				  "proxy-client"));
	scope_guard2<int, void *, int> spc_cu(segment_unmap_delayed, spc, 1);
	memset(spc, 0, sizeof(*spc));
	spc->plain_bipipe_ = plain_seg;

	if (eproc_gate.object) {
	    ssl_eproc_taint_cow(eproc_gate, eproc_seg, ssl_root_ct, 
				ssl_taint, &d->eproc_worker_args_);
	    d->eproc_started_ = 1;
	}
	scope_guard<int, thread_args *> 
	    worker_cu(thread_cleanup, &d->eproc_worker_args_, d->eproc_started_);

	// taint cow ssld and pass both bipipes
	ssld_taint_cow(ssld_gate, eproc_seg, cipher_seg, plain_seg, 
		       ssl_root_ct, ssl_taint, &d->ssld_worker_args_);
	d->ssld_started_ = 1;
	worker_cu.dismiss();
    } catch (std::exception &e) {
	sys_obj_unref(COBJ(d->base_ct_, d->ssl_ct_));
	throw e;
    }
    drop_star.dismiss();
    debug_print(dbg, "ssld_started %d eproc_started %d",
		d->ssld_started_, d->eproc_started_);
}

void
ssl_proxy_cleanup(ssl_proxy_descriptor *d)
{
    struct ssl_proxy_client *spc = 0;
    uint64_t bytes = sizeof(*spc);
    error_check(segment_map(d->client_seg_, 0, SEGMAP_READ, 
			    (void **)&spc, &bytes, 0));

    int64_t start = sys_clock_nsec();
    int64_t end = start + NSEC_PER_SECOND * 10;
    while (jos_atomic_read(&spc->ref_)) {
	sys_sync_wait(&jos_atomic_read(&spc->ref_), jos_atomic_read(&spc->ref_), end);
	if (end <= sys_clock_nsec()) {
	    cprintf("ssl_proxy_cleanup: timeout expired, cleaning up\n");
	    break;
	}
    }
    segment_unmap_delayed(spc, 1);

    sys_obj_unref(COBJ(d->base_ct_, d->ssl_ct_));
    thread_drop_star(d->taint_);    
    if (d->ssld_started_) {
	int r = thread_cleanup(&d->ssld_worker_args_);
	if (r < 0)
	    cprintf("ssl_proxy_cleanup: unable to unmap stack %s\n", e2s(r));
    }
    if (d->eproc_started_) {
	int r = thread_cleanup(&d->eproc_worker_args_);
	if (r < 0)
	    cprintf("ssl_proxy_cleanup: unable to unmap stack %s\n", e2s(r));
    }
    close(d->sock_fd_);
}

static int
ssl_proxy_select(int sock_fdnum, int cipher_fdnum, uint64_t nsec)
{
    struct Fd *sock_fd;
    error_check(fd_lookup(sock_fdnum, &sock_fd, 0, 0));
    struct Fd *cipher_fd;
    error_check(fd_lookup(cipher_fdnum, &cipher_fd, 0, 0));

    struct Dev *sock_dev;
    struct Dev *cipher_dev;
    error_check(dev_lookup(sock_fd->fd_dev_id, &sock_dev));
    error_check(dev_lookup(cipher_fd->fd_dev_id, &cipher_dev));

    static const int wstat_num = 2;
    struct wait_stat wstat[wstat_num];
    struct wait_stat *sock_ws = &wstat[0];
    struct wait_stat *cipher_ws = &wstat[1];
    
    int r = 0;
    memset(wstat, 0, sizeof(wstat));
    
    error_check(sock_dev->dev_statsync(sock_fd, dev_probe_read, sock_ws));
    error_check(cipher_dev->dev_statsync(cipher_fd, dev_probe_read, cipher_ws));
    
    int r0 = sock_dev->dev_probe(sock_fd, dev_probe_read);
    int r1 = cipher_dev->dev_probe(cipher_fd, dev_probe_read);
    error_check(r0);
    error_check(r1);
    
    if (r0)
	r |= SELECT_SOCK;
    if (r1)
	r |= SELECT_CIPHER;
    
    if (r)
	return r;

    int64_t t;
    error_check(t = sys_clock_nsec());
    error_check(multisync_wait(wstat, wstat_num, t + nsec));
    
    r0 = sock_dev->dev_probe(sock_fd, dev_probe_read);
    r1 = cipher_dev->dev_probe(cipher_fd, dev_probe_read);
    error_check(r0);
    error_check(r1);
    
    if (r0)
	r |= SELECT_SOCK;
    if (r1)
	r |= SELECT_CIPHER;
    
    return r;
}

static void
ssl_proxy_worker(int sock_fd, int cipher_fd)
{    
    char buf[4096];
    
    for (;;) {
	int r = ssl_proxy_select(sock_fd, cipher_fd, NSEC_PER_SECOND * 10);
	error_check(r);
	if (!r) {
	    cprintf("ssl_proxy::select timeout!\n");
	    break;
	}
	
	if (r & SELECT_SOCK) {
	    int r1 = read(sock_fd, buf, sizeof(buf));
	    if (r1 < 0) {
		cprintf("unknown read error: %d\n", r1);
		break;
	    } else if (!r1) {
		debug_cprint(dbg, "stopping -- socket fd closed");
		break;
	    } else {
		int r2 = write(cipher_fd, buf, r1);
		if (r2 < 0) {
		    if (errno == EPIPE) {
			debug_cprint(dbg, "stopping -- cipher fd closed");
			break;
		    } else {
			cprintf("unknown write error: %d\n", r2);
			break;
		    }
		}
		// XXX
		assert(r1 == r2);
	    }
	}
	
	if (r & SELECT_CIPHER) {
	    int r1 = read(cipher_fd, buf, sizeof(buf));
	    if (!r1) {
		debug_cprint(dbg, "stopping -- cipher fd closed");
		break;
	    } else if (r1 < 0) {
		cprintf("http_ssl_proxy: unknown read error: %d\n", r1);
		break;
	    } else {
		int r2 = write(sock_fd, buf, r1);
		if (r2 < 0) {
		    if (errno == ENOTCONN) {
			debug_cprint(dbg, "stopping -- sock fd closed");
			break;
		    }
		    cprintf("unknown write error: %d\n", r2);
		    break;
		}
		// XXX
		assert(r1 == r2);
	    }
	}
    }
    return;
}

static void
ssl_proxy_stub_cleanup(void *a)
{
    ssl_proxy_descriptor *d = (ssl_proxy_descriptor *) a;
    ssl_proxy_loop(d, 1);
}

static void
ssl_proxy_stub(void *a)
{
    ssl_proxy_descriptor *d = (ssl_proxy_descriptor *) a;    
    ssl_proxy_loop(d, 0);
}

void 
ssl_proxy_loop(ssl_proxy_descriptor *d, char cleanup)
{
    int cipher_fd;
    // NONBLOCK to avoid potential deadlock with ssld
    errno_check(cipher_fd = bipipe_fd(d->cipher_bipipe_, 
				      ssl_proxy_bipipe_client, 
				      O_NONBLOCK, 0, 0));
    scope_guard<void, ssl_proxy_descriptor *> 
	cu1(ssl_proxy_cleanup, d, cleanup);
    scope_guard<int, int> cu0(close, cipher_fd);

    ssl_proxy_worker(d->sock_fd_, cipher_fd);
}

void 
ssl_proxy_thread(ssl_proxy_descriptor *d, char cleanup)
{
    debug_print(dbg, "cleanup %d", cleanup);
    struct cobj_ref t;
    if (cleanup) {
	error_check(thread_create_option(start_env->proc_container, 
					 &ssl_proxy_stub_cleanup,
					 d, sizeof(*d),
					 &t, "ssl-proxy", 0, THREAD_OPT_ARGCOPY));
    } else {
	error_check(thread_create_option(start_env->proc_container, 
					 &ssl_proxy_stub,
					 d, sizeof(*d),
					 &t, "ssl-proxy", 0, THREAD_OPT_ARGCOPY));
    }
}

int 
ssl_proxy_client_fd(cobj_ref plain_seg)
{
    try {
	struct ssl_proxy_client *spc = 0;
	uint64_t bytes = sizeof(*spc);
	error_check(segment_map(plain_seg, 0, SEGMAP_READ | SEGMAP_WRITE, 
				(void **)&spc, &bytes, 0));
	scope_guard2<int, void *, int> spc_cu(segment_unmap_delayed, spc, 0);	
	int s;
	error_check(s = bipipe_fd(spc->plain_bipipe_, ssl_proxy_bipipe_client, 0, 0, 0));
	jos_atomic_inc64(&spc->ref_);
	return s;
    } catch (basic_exception &e) {
	cprintf("ssl_proxy_client_fd: error: %s\n", e.what());
	return -1;
    }
}

void 
ssl_proxy_client_done(cobj_ref plain_seg)
{
    try {
	struct ssl_proxy_client *spc = 0;
	uint64_t bytes = sizeof(*spc);
	error_check(segment_map(plain_seg, 0, SEGMAP_READ | SEGMAP_WRITE, 
				(void **)&spc, &bytes, 0));
	scope_guard2<int, void *, int> spc_cu(segment_unmap_delayed, spc, 0);	
	jos_atomic_dec64(&spc->ref_);
    } catch (basic_exception &e) {
	cprintf("ssl_proxy_client_done: error: %s\n", e.what());
    }
    return;
}
