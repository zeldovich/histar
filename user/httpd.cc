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

struct proxy_args {
    int cipher_fd;
    int sock_fd;
    struct cobj_ref ssl_root_obj;
};

static const char ssl_mode = 0;
static const char proxy_dbg = 0;

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

static  void
http_ssl_proxy(void *a)
{
    struct proxy_args *pa = (struct proxy_args *) a;
    int cipher_fd = pa->cipher_fd;
    int sock_fd = pa->sock_fd;
    struct cobj_ref ssl_root_obj = pa->ssl_root_obj;
    free(a);

    fd_set readset, writeset, exceptset;
    int maxfd = MAX(cipher_fd, sock_fd) + 1;
    char buf[4096];
    for (;;) {
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&exceptset);
	FD_SET(sock_fd, &readset);	
	FD_SET(cipher_fd, &readset);	
	
	int r = select(maxfd, &readset, &writeset, &exceptset, 0);
	if (r < 0) {
	    cprintf("unknown select error: %s\n", strerror(errno));
	    continue;
	}
	
	if (FD_ISSET(sock_fd, &readset)) {
	    int r1 = read(sock_fd, buf, sizeof(buf));
	    if (r1 < 0) {
		cprintf("unknown read error: %d\n", r1);
	    } else {
		int r2 = write(cipher_fd, buf, r1);
		if (r2 < 0) {
		    if (errno == EPIPE) {
			// other end of cipher_fd closed
			close(cipher_fd);
			close(sock_fd);
			debug_cprint(proxy_dbg, "stopping -- cipher fd closed");
			break;
		    } else {
			cprintf("unknown write error: %d\n", r2);
		    }
		}
		// XXX
		assert(r1 == r2);
	    }
	}
	if (FD_ISSET(cipher_fd, &readset)) {
	    int r1 = read(cipher_fd, buf, sizeof(buf));
	    if (!r1) {
		// other end of cipher_fd closed
		close(cipher_fd);
		close(sock_fd);
		debug_cprint(proxy_dbg, "stopping -- cipher fd closed");
		break;
	    } else if (r1 < 0) {
		cprintf("http_ssl_proxy: unknown read error: %d\n", r1);
	    } else {
		int r2 = write(sock_fd, buf, r1);
		// XXX
		assert(r1 == r2);
	    }
	}
    }    
    sys_obj_unref(ssl_root_obj);
}

static uint64_t
http_init_client(uint64_t base_ct, int fd[2])
{
    uint64_t ssl_taint = handle_alloc();
    label ssl_root_label(1);
    ssl_root_label.set(ssl_taint, 3);
    
    int64_t ssl_root_ct = sys_container_alloc(base_ct,
					      ssl_root_label.to_ulabel(),
					      "ssl-root", 0, CT_QUOTA_INF);
    error_check(ssl_root_ct);
    
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

    fd[0] = cipher_fd;
    fd[1] = plain_fd;

    // taint cow ssld and pass both bipipes
    ssld_taint_cow(get_ssld_cow(), cipher_seg, plain_seg, 
		   ssl_root_ct, ssl_taint);
    
    return ssl_root_ct;
}

static void
http_client(void *arg)
{
    char buf[512];
    int r;
    int s = (int64_t) arg;

    int fd[2];
    uint64_t ssl_ct = http_init_client(start_env->shared_container, fd);
    struct cobj_ref ssl_root_obj = COBJ(start_env->shared_container, ssl_ct);
    
    struct proxy_args *pa = 
	(struct proxy_args*) malloc(sizeof(struct proxy_args));

    pa->cipher_fd = fd[0];
    pa->sock_fd = s;
    pa->ssl_root_obj = ssl_root_obj;
    
    struct cobj_ref t;
    r = thread_create(start_env->proc_container, &http_ssl_proxy,
		      (void*) pa, &t, "http-ssl-proxy");
    error_check(r);

    s = fd[1];

    try {
	tcpconn tc(s);
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

	snprintf(buf, sizeof(buf),
		"HTTP/1.0 401 Forbidden\r\n"
		"WWW-Authenticate: Basic realm=\"jos-httpd\"\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<h1>Please log in.</h1>\r\n");

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
