extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/ssld.h>

#include <string.h>
#include <unistd.h>

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
#include <inc/netdclnt.hh>

static const char ssl_mode = 1;

static struct cobj_ref
get_netd_util(void)
{
    struct fs_inode netd_ct_ino;
    error_check(fs_namei("/netd", &netd_ct_ino));
    uint64_t netd_ct = netd_ct_ino.obj.object;

    int64_t gate_id;
    error_check(gate_id = container_find(netd_ct, kobj_gate, "netd-util"));
    return COBJ(netd_ct, gate_id);
}

static void
http_client(void *arg)
{
    char buf[512];
    int s = (int64_t) arg;
    
    // XXX
    if (ssl_mode) {
	uint64_t ssl_taint = handle_alloc();
	label ssl_root_label(1);
	ssl_root_label.set(ssl_taint, 2);
	int64_t ssl_root = sys_container_alloc(start_env->shared_container,
					       ssl_root_label.to_ulabel(),
					       "ssl-root", 0, CT_QUOTA_INF);
	error_check(ssl_root);
	label netd_ds(3);
	netd_ds.set(ssl_taint, LB_LEVEL_STAR);
	netd_create_gates(get_netd_util(), ssl_root, 0, &netd_ds, 0);
	
	label ssld_cs(LB_LEVEL_STAR);
	ssld_cs.set(ssl_taint, 2);
	struct cobj_ref ssld = ssld_shared_server();
	struct cobj_ref ssld_cow = ssld_shared_cow();

	struct cobj_ref ssld_tainted = 
	    ssld_cow_call(ssld_cow, ssl_root, &ssld_cs, 0, 0);
	
	s = ssl_accept(s, ssl_root, ssld_tainted);
	if (s < 0)
	    throw basic_exception("unable to alloc ssl_socket: %s", e2s(s));
    }

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
    http_server();
}
