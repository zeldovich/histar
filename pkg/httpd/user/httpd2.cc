extern "C" {
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/bipipe.h>
#include <inc/debug.h>
#include <inc/argv.h>

#include <string.h>
#include <errno.h>

#include <sys/socket.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/sslproxy.hh>

#include <inc/a2pdf.hh>
#include <inc/perl.hh>
#include <inc/wrap.hh>

#include <iostream>
#include <sstream>

static const char dbg = 0;

static char http_auth_enable;
static fs_inode httpd_root_ino;

arg_desc cmdarg[] = {
    { "http_auth_enable", "0" },
    { "httpd_root_path", "/www" },
        
    { 0, 0 }
};

static void
http_on_request(tcpconn *tc, const char *req, uint64_t ut, uint64_t ug)
{
    std::ostringstream header;

    // XXX wrap stuff has no timeout
    if (!memcmp(req, "/cgi-bin/", strlen("/cgi-bin/"))) {
	std::string pn = req;
	perl(httpd_root_ino, pn.c_str(), ut, header);
    } else if (strcmp(req, "/")) {
	std::string pn = req;
	a2pdf(httpd_root_ino, pn.c_str(), ut, header);
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
http_client(int s)
{
    char buf[512];

    try {
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

int
main(int ac, const char **av)
{
    if (ac < 3) {
	cprintf("usage: %s bipipe-container bipipe-object\n", av[0]);
	return -1;
    }
    
    uint64_t c, o;

    int r;
    r = strtou64(av[1], 0, 10, &c);
    if (r < 0)
	panic("parsing container id %s: %s", av[1], e2s(r));

    r = strtou64(av[2], 0, 10, &o);
    if (r < 0)
	panic("parsing object id %s: %s", av[2], e2s(r));

    http_auth_enable = atoi(arg_val(cmdarg, "http_auth_enable"));
    const char * httpd_root_path = arg_val(cmdarg, "httpd_root_path");
    error_check(fs_namei(httpd_root_path, &httpd_root_ino));
    
    if (dbg) {
	printf("httpd2: config:\n");
	printf(" %-20s %ld.%ld\n", "bipipe_seg", c, o);
	printf(" %-20s %d\n", "http_auth_enable", http_auth_enable);
	printf(" %-20s %s\n", "httpd_root_path", httpd_root_path);
    }
    
    try {
	int s;
	error_check(s = bipipe_fd(COBJ(c, o), ssl_proxy_bipipe_client, 0, 0, 0));
	scope_guard<int, int> close_bipipe(close, s);
	http_client(s);
    } catch (basic_exception &e) {
	cprintf("httpd: %s\n", e.what());
	return -1;
    }
    return 0;

}
