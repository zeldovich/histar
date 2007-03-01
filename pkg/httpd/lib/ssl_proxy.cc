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
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/ssldclnt.hh>

static const char proxy_dbg = 0;

ssl_proxy::ssl_proxy(struct cobj_ref ssld_gate, struct cobj_ref eproc_gate, 
		     uint64_t base_ct, int sock_fd)
{
    proxy_started_ = 0;
    ssld_started_ = 0;
    eproc_started_ = 0;
    plain_fd_ = 0;
    ssld_gate_ = ssld_gate;
    eproc_gate_ = eproc_gate;
    nfo_ = (struct info*)malloc(sizeof(*nfo_));
    if (nfo_ < 0)
	throw error(-E_NO_MEM, "cannot malloc");
    memset(nfo_, 0, sizeof(*nfo_));
    nfo_->base_ct_ = base_ct;
    nfo_->sock_fd_ = sock_fd;
    atomic_inc64(&nfo_->ref_count_);
}

ssl_proxy::~ssl_proxy(void)
{
    // if started_, proxy thread is responsible for closing sock_fd 
    // and cipher_fd
    if (proxy_started_)
	close(plain_fd_);
    else {
	close(nfo_->sock_fd_);
	if (nfo_->cipher_fd_)
	    close(nfo_->cipher_fd_);
	if (plain_fd_)
	    close(plain_fd_);
    }

    if (ssld_started_) {
	int r = thread_cleanup(&ssld_worker_args_);
	if (r < 0)
	    cprintf("ssl_proxy::~ssl_proxy: unable to unmap stack %s\n", e2s(r));
    }

    if (eproc_started_) {
	int r = thread_cleanup(&eproc_worker_args_);
	if (r < 0)
	    cprintf("ssl_proxy::~ssl_proxy: unable to unmap stack %s\n", e2s(r));
    }

    cleanup(nfo_);
}

void
ssl_proxy::cleanup(struct info *nfo)
{
    if (atomic_dec_and_test((atomic_t *)&nfo->ref_count_)) {
	if (nfo->ssl_ct_)
	    sys_obj_unref(COBJ(nfo->base_ct_, nfo->ssl_ct_));
	free(nfo);
    }
}

int
ssl_proxy::plain_fd(void)
{
    if (!proxy_started_)
	throw basic_exception("proxy isn't running");
    return plain_fd_;
}

int
ssl_proxy::select(int sock_fdnum, int cipher_fdnum, uint64_t msec)
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
    error_check(t = sys_clock_msec());
    error_check(multisync_wait(wstat, wstat_num, t + msec));
    
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
 

void 
ssl_proxy::proxy_thread(void *a)
{
    struct info *nfo = (struct info*)a;
    int cipher_fd = nfo->cipher_fd_;
    int sock_fd = nfo->sock_fd_;
    char buf[4096];
    
    scope_guard<void, struct info *> cu2(cleanup, nfo);
    scope_guard<int, int> cu1(close, cipher_fd);
    scope_guard<int, int> cu0(close, sock_fd);
    
    for (;;) {
	int r = select(sock_fd, cipher_fd, 10000);
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
		debug_cprint(proxy_dbg, "stopping -- socket fd closed");
		break;
	    } else {
		int r2 = write(cipher_fd, buf, r1);
		if (r2 < 0) {
		    if (errno == EPIPE) {
			debug_cprint(proxy_dbg, "stopping -- cipher fd closed");
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
		debug_cprint(proxy_dbg, "stopping -- cipher fd closed");
		break;
	    } else if (r1 < 0) {
		cprintf("http_ssl_proxy: unknown read error: %d\n", r1);
		break;
	    } else {
		int r2 = write(sock_fd, buf, r1);
		if (r2 < 0) {
		    if (errno == ENOTCONN) {
			debug_cprint(proxy_dbg, "stopping -- sock fd closed");
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

void
ssl_proxy::start(void)
{
    uint64_t ssl_taint = handle_alloc();
    label ssl_root_label(1);
    ssl_root_label.set(ssl_taint, 3);

    int64_t ssl_root_ct = sys_container_alloc(nfo_->base_ct_,
					      ssl_root_label.to_ulabel(),
					      "ssl-root", 0, CT_QUOTA_INF);
    error_check(ssl_root_ct);
    nfo_->ssl_ct_ = ssl_root_ct;

    try {
	// manually setup bipipe segments
	struct cobj_ref cipher_seg;
	label cipher_label(1);
	cipher_label.set(ssl_taint, 3);
	error_check(bipipe_alloc(ssl_root_ct, &cipher_seg, 
				 cipher_label.to_ulabel(), "cipher-bipipe"));
	
	struct cobj_ref plain_seg;
	label plain_label(1);
	plain_label.set(ssl_taint, 3);
	error_check(bipipe_alloc(ssl_root_ct,&plain_seg, 
				 plain_label.to_ulabel(), "plain-bipipe"));
	
	struct cobj_ref eproc_seg = COBJ(0, 0);
	if (eproc_gate_.object) {
	    label eproc_label(1);
	    eproc_label.set(ssl_taint, 3);
	    error_check(bipipe_alloc(ssl_root_ct, &eproc_seg, 
				     eproc_label.to_ulabel(), "eproc-bipipe"));
	}

	// NONBLOCK to avoid potential deadlock with ssld
	int cipher_fd = bipipe_fd(cipher_seg, 0, O_NONBLOCK, ssl_taint, 0);
	int plain_fd = bipipe_fd(plain_seg, 0, 0, ssl_taint, 0);
	error_check(cipher_fd);
	error_check(plain_fd);

	nfo_->cipher_fd_ = cipher_fd;
	plain_fd_ = plain_fd;

	if (eproc_gate_.object) {
	    ssl_eproc_taint_cow(eproc_gate_, eproc_seg, ssl_root_ct, ssl_taint, &eproc_worker_args_);
	    eproc_started_ = 1;
	}

	// taint cow ssld and pass both bipipes
	ssld_taint_cow(ssld_gate_, eproc_seg, cipher_seg, plain_seg, 
		       ssl_root_ct, ssl_taint, &ssld_worker_args_);
	ssld_started_ = 1;
    } catch (std::exception &e) {
	sys_obj_unref(COBJ(nfo_->base_ct_, nfo_->ssl_ct_));
	throw e;
    }

    atomic_inc64(&nfo_->ref_count_);

    struct cobj_ref t;
    int r = thread_create(start_env->proc_container, &proxy_thread,
			  nfo_, &t, "ssl-proxy");
    if (r < 0) {
	atomic_dec((atomic_t *)&nfo_->ref_count_);
	throw error(r, "can't start proxy thread");
    }
    proxy_started_ = 1;
}
