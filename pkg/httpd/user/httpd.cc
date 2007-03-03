extern "C" {
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/bipipe.h>
#include <inc/debug.h>
#include <inc/ssl_fd.h>
#include <inc/argv.h>

#include <sys/socket.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/sslproxy.hh>
#include <inc/module.hh>

#include <iostream>
#include <sstream>

static char ssl_enable;
static char ssl_privsep_enable;
static char ssl_eproc_enable;
static char http_auth_enable;
static fs_inode httpd_root_ino;

arg_desc cmdarg[] = {
    { "ssl_enable", "1" },
    { "ssl_privsep_enable", "1" },
    { "ssl_eproc_enable", "1" },
    
    { "ssl_server_pem", "/bin/server.pem" },
    { "ssl_dh_pem", "/bin/dh.pem" },
    { "ssl_servkey_pem", "/bin/servkey.pem" },

    { "http_auth_enable", "0" },
    { "httpd_root_path", "/www" },
        
    { 0, 0 }
};

static struct cobj_ref the_ssld_cow;
static struct cobj_ref the_eprocd_cow;

static void *the_ctx;

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

static struct cobj_ref
get_eprocd_cow(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/httpd/ssl_eprocd/", &ct_ino));
    uint64_t eproc_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(eproc_ct, kobj_gate, "eproc-cow"));
    return COBJ(eproc_ct, gate_id);
}

static void
http_on_request(tcpconn *tc, const char *req, uint64_t ut, uint64_t ug)
{
    std::ostringstream header;

    // XXX wrap stuff has no timeout
    if (!memcmp(req, "/cgi-bin/", strlen("/cgi-bin/"))) {
	std::string pn = req;
	perl(httpd_root_ino, pn.c_str(), ut, ug, header);
    } else if (strcmp(req, "/")) {
	std::string pn = req;
	a2pdf(httpd_root_ino, pn.c_str(), ut, ug, header);
    } else {
	header << "HTTP/1.0 500 Server error\r\n";
	header << "Content-Type: text/html\r\n";
	header << "\r\n";
	header << "<h1>unknown request</h1>\r\n";
    }

    std::string reply = header.str();
    tc->write(reply.data(), reply.size());
}

static void
http_client(void *arg)
{
    char buf[512];
    int sock_fd = (int64_t) arg;
    scope_guard<int, int> close_sock(close, sock_fd);

    try {
	int s = sock_fd;
	
	if (ssl_enable && ssl_privsep_enable) {
	    ssl_proxy_descriptor d;
	    ssl_proxy_alloc(the_ssld_cow, the_eprocd_cow, 
			    start_env->shared_container, sock_fd, &d);
	    ssl_proxy_thread(&d, 1);
	    s = bipipe_fd(d.plain_bipipe_, ssl_proxy_bipipe_client, 0, 0, 0);
	    if (s < 0) {
		// proxy thread will close sock
		close_sock.dismiss();
		throw basic_exception("unable to open plain bipipe: %s\n", 
				      strerror(errno));
	    }	    
	} else if (ssl_enable) {
	    error_check(s = ssl_accept(the_ctx, s));
	}
	close_sock.dismiss();
	scope_guard<int, int> close_s(close, s);
	
	tcpconn tc(s, 0);
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

	try {
	    if (auth[0]) {
		char *authdata = base64_decode(&auth[0]);
		scope_guard<void, void*> free_ad(free, authdata);
		if (authdata == 0)
		    throw error(-E_NO_MEM, "base64_decode");

		char *colon = strchr(authdata, ':');
		if (colon == 0)
		    throw basic_exception("badly formatted authorization data");

		*colon = 0;
		char *user = authdata;
		char *pass = colon + 1;

		uint64_t ug, ut;
		try {
		    auth_login(user, pass, &ug, &ut);
		} catch (std::exception &e) {
		    snprintf(buf, sizeof(buf),
			    "HTTP/1.0 401 Forbidden\r\n"
			    "WWW-Authenticate: Basic realm=\"jos-httpd\"\r\n"
			    "Content-Type: text/html\r\n"
			    "\r\n"
			    "<h1>Could not log in</h1>\r\n"
			    "%s\r\n", e.what());
		    tc.write(buf, strlen(buf));
		    return;
		}

		http_on_request(&tc, pnbuf, ut, ug);
		return;
	    }

	    if (http_auth_enable) {
		snprintf(buf, sizeof(buf),
			"HTTP/1.0 401 Forbidden\r\n"
			"WWW-Authenticate: Basic realm=\"jos-httpd\"\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<h1>Please log in.</h1>\r\n");
		tc.write(buf, strlen(buf));
	    } else {
		http_on_request(&tc, pnbuf, 0, 0);
	    }
	} catch (std::exception &e) {
	    snprintf(buf, sizeof(buf),
		    "HTTP/1.0 500 Server error\r\n"
		    "Content-Type: text/html\r\n"
		    "\r\n"
		    "<h1>Server error.</h1>\r\n"
		    "%s", e.what());
	    tc.write(buf, strlen(buf));
	}
    } catch (std::exception &e) {
	printf("http_client: %s\n", e.what());
    }
}

static void __attribute__((noreturn))
http_server(uint16_t port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("httpd: server on port %d\n", port);
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
main(int ac, const char **av)
{
    uint16_t port = 80;

    error_check(argv_parse(ac, av, cmdarg));

    ssl_enable = atoi(arg_val(cmdarg, "ssl_enable"));
    ssl_privsep_enable = atoi(arg_val(cmdarg, "ssl_privsep_enable"));
    ssl_eproc_enable = atoi(arg_val(cmdarg, "ssl_eproc_enable"));
    http_auth_enable = atoi(arg_val(cmdarg, "http_auth_enable"));

    const char * httpd_root_path = arg_val(cmdarg, "httpd_root_path");
    error_check(fs_namei(httpd_root_path, &httpd_root_ino));

    printf("httpd: config:\n");
    printf(" %-20s %d\n", "ssl_enable", ssl_enable);
    printf(" %-20s %d\n", "ssl_privsep_enable", ssl_privsep_enable);
    printf(" %-20s %d\n", "ssl_eproc_enable", ssl_eproc_enable);
    printf(" %-20s %d\n", "http_auth_enable", http_auth_enable);
    printf(" %-20s %s\n", "httpd_root_path", httpd_root_path);

    if (ssl_enable && ssl_privsep_enable) {
	the_ssld_cow = get_ssld_cow();
	try {
	    the_eprocd_cow = get_eprocd_cow();
	} catch (error &e) {
	    if (e.err() == -E_NOT_FOUND) {
		printf("httpd: unable to find eprocd\n");
		the_eprocd_cow = COBJ(0, 0);
	    }
	    else {
		printf("httpd: %s\n", e.what());
		return -1;
	    }
	}
	port = 443;
    } else if (ssl_enable) {
	const char *server_pem = arg_val(cmdarg, "ssl_server_pem");
	const char *servkey_pem = arg_val(cmdarg, "ssl_servkey_pem");
	const char *dh_pem = arg_val(cmdarg, "ssl_dh_pem");
	
	printf("httpd: ssl files:\n");
	printf(" %-20s %s\n", "server_pem", server_pem);
	printf(" %-20s %s\n", "servkey_pem", servkey_pem);
	printf(" %-20s %s\n", "dh_pem", dh_pem);
	
	error_check(ssl_init(server_pem, dh_pem, servkey_pem, &the_ctx));
	port = 443;
    }
    
    http_server(port);
}
