extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/bipipe.h>
#include <inc/debug.h>

#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/spawn.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/ssldclnt.hh>

static const char proxy_dbg = 1;

class ssl_proxy
{
public:
    ssl_proxy(struct cobj_ref ssld_gate, uint64_t base_ct, int sock_fd);
    ~ssl_proxy(void);

    void start(void);
    int plain_fd(void);

private:
    struct info {
	int cipher_fd_;
	int sock_fd_;
	uint64_t base_ct_;
	uint64_t ssl_ct_;
	atomic64_t ref_count_;
    } *nfo_;
    struct cobj_ref ssld_gate_;
    uint64_t plain_fd_;
    char started_;
    
    static void proxy_thread(void *a);
    static void cleanup(struct info *nfo);
    static int select(int sock_fd, int cipher_fd, uint64_t msec);

    static const uint32_t SELECT_SOCK =   0x0001;
    static const uint32_t SELECT_CIPHER = 0x0002;
};

ssl_proxy::ssl_proxy(struct cobj_ref ssld_gate, uint64_t base_ct, int sock_fd)
{
    started_ = 0;
    ssld_gate_ = ssld_gate;
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
    if (started_)
	close(plain_fd_);
    else
	close(nfo_->sock_fd_);
    cleanup(nfo_);
}

void
ssl_proxy::cleanup(struct info *nfo)
{
    if (atomic_dec_and_test((atomic_t *)&nfo->ref_count_)) {
	sys_obj_unref(COBJ(nfo->base_ct_, nfo->ssl_ct_));
	free(nfo);
    }
}

int
ssl_proxy::plain_fd(void)
{
    if (!started_)
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
	struct bipipe_seg *cipher_bs = 0;
	label cipher_label(1);
	cipher_label.set(ssl_taint, 3);
	error_check(segment_alloc(ssl_root_ct,
				  sizeof(*cipher_bs), &cipher_seg, 
				  (void **)&cipher_bs, cipher_label.to_ulabel(), 
			      "cipher-bipipe"));
	scope_guard<int, void*> umap(segment_unmap, cipher_bs);
	memset(cipher_bs, 0, sizeof(*cipher_bs));
	cipher_bs->p[0].open = 1;
	cipher_bs->p[1].open = 1;
	
	struct cobj_ref plain_seg;
	struct bipipe_seg *plain_bs = 0;
	label plain_label(1);
	plain_label.set(ssl_taint, 3);
	error_check(segment_alloc(ssl_root_ct,
				  sizeof(*plain_bs), &plain_seg, 
				  (void **)&plain_bs, plain_label.to_ulabel(), 
				  "plain-bipipe"));
	scope_guard<int, void*> umap2(segment_unmap, plain_bs);
	memset(plain_bs, 0, sizeof(*plain_bs));
	plain_bs->p[0].open = 1;
	plain_bs->p[1].open = 1;
	
	// NONBLOCK to avoid potential deadlock with ssld
	int cipher_fd = bipipe_fd(cipher_seg, 0, O_NONBLOCK);
	int plain_fd = bipipe_fd(plain_seg, 0, 0);
	error_check(cipher_fd);
	error_check(plain_fd);
	
	nfo_->cipher_fd_ = cipher_fd;
	plain_fd_ = plain_fd;

	// taint cow ssld and pass both bipipes
	ssld_taint_cow(ssld_gate_, cipher_seg, plain_seg, 
		       ssl_root_ct, ssl_taint);
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
    started_ = 1;
}

/////
// httpd code
/////

static const char ssl_mode = 1;
static const char ask_for_auth = 1;

static uint64_t ssld_access_grant;

static struct cobj_ref
get_ssld_cow(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/httpd/ssld/", &ct_ino));
    uint64_t ssld_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(ssld_ct, kobj_gate, "ssld-cow"));
    return COBJ(ssld_ct, gate_id);
}

static void
http_client(void *arg)
{
    char buf[512];
    int sock_fd = (int64_t) arg;

    try {
	ssl_proxy proxy(get_ssld_cow(), start_env->shared_container, sock_fd);
	int s = sock_fd;
	if (ssl_mode) {
	    proxy.start();
	    s = proxy.plain_fd();
	}
	
	tcpconn tc(s, ssl_mode ? 0 : 1);
	lineparser lp(&tc);

	const char *req = lp.read_line();
	if (req == 0 || strncmp(req, "GET ", 4))
	    throw basic_exception("bad http request: %s", req);

	const char *pn_start = req + 4;
	char *space = strchr(pn_start, ' ');
	if (space == 0)
	    throw basic_exception("no space in http req: %s", req);

	char pnbuf[256];
	strncpy(&pnbuf[0], pn_start, space - pn_start);
	pnbuf[sizeof(pnbuf) - 1] = '\0';

	char auth[64];
	auth[0] = '\0';

	while (req[0] != '\0') {
	    req = lp.read_line();
	    if (req == 0)
		throw basic_exception("client EOF");

	    const char *auth_prefix = "Authorization: Basic ";
	    if (!strncmp(req, auth_prefix, strlen(auth_prefix))) {
		strncpy(&auth[0], req + strlen(auth_prefix), sizeof(auth));
		auth[sizeof(auth) - 1] = '\0';
	    }
	}

	if (auth[0]) {
	    char *authdata = base64_decode(&auth[0]);
	    if (authdata == 0)
		throw error(-E_NO_MEM, "base64_decode");

	    char *colon = strchr(authdata, ':');
	    if (colon == 0)
		throw basic_exception("badly formatted authorization data");

	    *colon = 0;
	    char *user = authdata;
	    char *pass = colon + 1;

	    uint64_t ug, ut;
	    int auth_ok = 0;
	    try {
		auth_login(user, pass, &ug, &ut);
		auth_ok = 1;
	    } catch (std::exception &e) {
		cprintf("httpd: cannot login: %s\n", e.what());
	    }

	    if (auth_ok) {
		int64_t worker_ct, worker_gt;
		error_check(worker_ct = container_find(start_env->root_container, kobj_container, "httpd_worker"));
		error_check(worker_gt = container_find(worker_ct, kobj_gate, "worker"));

		label cs(LB_LEVEL_STAR);
		cs.set(ut, 3);

		label dr(0);
		dr.set(ut, 3);

		gate_call_data gcd;
		strncpy(&gcd.param_buf[0], &pnbuf[0], sizeof(gcd.param_buf));
		gate_call gc(COBJ(worker_ct, worker_gt), &cs, 0, &dr);
		gc.call(&gcd, 0);

		void *va = 0;
		uint64_t len;
		error_check(segment_map(gcd.param_obj, 0, SEGMAP_READ, &va, &len, 0));
		scope_guard<int, void *> unmap(segment_unmap, va);

		tc.write((const char *) va, len);
		return;
	    }
	}

	if (ask_for_auth) {
	    snprintf(buf, sizeof(buf),
		    "HTTP/1.0 401 Forbidden\r\n"
		    "WWW-Authenticate: Basic realm=\"jos-httpd\"\r\n"
		    "Content-Type: text/html\r\n"
		    "\r\n"
		    "<h1>Please log in.</h1>\r\n");
	} else {
	    snprintf(buf, sizeof(buf),
		    "HTTP/1.0 200 OK\r\n"
		    "Content-Type: text/html\r\n"
		    "\r\n"
		    "<h1>Hello world.</h1>\r\n");
	}

	tc.write(buf, strlen(buf));
    } catch (std::exception &e) {
	printf("http_client: %s\n", e.what());
    }
    
}

static void __attribute__((noreturn))
http_server(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(80);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("httpd: server on port 80\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	struct cobj_ref t;
	r = thread_create(start_env->proc_container, &http_client,
			  (void*) (int64_t) ss, &t, "http client");
	
	if (r < 0) {
	    printf("cannot spawn client thread: %s\n", e2s(r));
	    close(ss);
	} else {
	    fd_give_up_privilege(ss);
	}
    }
}

int
main(int ac, char **av)
{
    if (ssl_mode) {
	if (ac < 2) {
	    printf("httpd: error: access grant required in ssl mode\n");
	    return -1;
	}
	error_check(strtou64(av[1], 0, 10, &ssld_access_grant));
    }
    http_server();
}
