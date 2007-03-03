extern "C" {
#include <inc/string.h>
#include <inc/error.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/bipipe.h>
#include <inc/debug.h>
#include <inc/argv.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

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
#include <inc/module.hh>

#include <dj/gatesender.hh>
#include <dj/djautorpc.hh>
#include <dj/djutil.hh>
#include <dj/miscx.h>

#include <iostream>
#include <sstream>

static const char dbg = 0;

static char http_auth_enable;
static fs_inode httpd_root_ino;
static int httpd_dj_enable = 0;

static gate_sender *the_gs;
static dj_global_cache djcache;

/* Config state for DJ */
static dj_pubkey dj_server_pk;
static uint64_t dj_server_ct;
static dj_gatename dj_authgate;
static dj_gatename dj_appgate;

/* State associated with DJ calls */
static dj_gcat dj_ut, dj_ug;
static uint64_t dj_calltaint;

static void
http_on_request(tcpconn *tc, const char *req, uint64_t ut, uint64_t ug)
{
    char *tmp;
    char strip_req[256];
    strncpy(strip_req, req, sizeof(strip_req) - 1);
    strip_req[sizeof(strip_req) - 1] = 0;
    
    std::ostringstream header;

    // XXX wrap stuff has no timeout
    if (httpd_dj_enable) {
	container_alloc_req ct_req;
	container_alloc_res ct_res;

	ct_req.parent = dj_server_ct;
	ct_req.quota = CT_QUOTA_INF;
	ct_req.timeout_msec = 10000;
	ct_req.label.ents.push_back(dj_ut);

	dj_message_endpoint ctalloc_ep;
	ctalloc_ep.set_type(EP_GATE);
	ctalloc_ep.ep_gate->msg_ct = dj_server_ct;
	ctalloc_ep.ep_gate->gate.gate_ct = 0;
	ctalloc_ep.ep_gate->gate.gate_id = GSPEC_CTALLOC;

	label ct_grant(3);
	ct_grant.set(ut, LB_LEVEL_STAR);
	ct_grant.set(dj_calltaint, LB_LEVEL_STAR);

	label ct_clear(0);
	ct_clear.set(ut, 3);
	ct_clear.set(dj_calltaint, 3);

	dj_autorpc arpc(the_gs, 5, dj_server_pk, djcache);
	dj_delivery_code c = arpc.call(ctalloc_ep, ct_req, ct_res,
				       0, &ct_grant, &ct_clear);
	if (c != DELIVERY_DONE)
	    throw basic_exception("ctallocd: %d", c);

	error_check(ct_res.ct_id);
	uint64_t userct = ct_res.ct_id;

	webapp_arg web_arg;
	webapp_res web_res;
	web_arg.reqpath = req;

	dj_message_endpoint webapp_ep;
	webapp_ep.set_type(EP_GATE);
	webapp_ep.ep_gate->msg_ct = userct;
	webapp_ep.ep_gate->gate = dj_appgate;

	label web_taint(1);
	web_taint.set(ug, 0);
	web_taint.set(ut, 3);

	label web_grant(3);
	web_grant.set(ug, LB_LEVEL_STAR);
	web_grant.set(dj_calltaint, LB_LEVEL_STAR);

	label web_clear(0);
	web_clear.set(ut, 3);
	web_clear.set(dj_calltaint, 3);

	c = arpc.call(webapp_ep, web_arg, web_res,
		      &web_taint, &web_grant, &web_clear);
	if (c != DELIVERY_DONE)
	    throw basic_exception("webapp: %d\n", c);

	header << web_res.httpres;
    } else if (!memcmp(req, "/cgi-bin/", strlen("/cgi-bin/"))) {
	std::string pn = req;
	perl(httpd_root_ino, pn.c_str(), ut, ug, header);
    } else if (tmp = strchr(strip_req, '?')) {
	*tmp = 0;
	tmp++;
	if (!strcmp(tmp, "a2pdf")) {
	    std::string pn = strip_req;
	    a2pdf(httpd_root_ino, pn.c_str(), ut, ug, header);
	} else if (!strcmp(tmp, "cat")) {
	    std::string pn = strip_req;
	    cat(httpd_root_ino, pn.c_str(), ut, ug, header);
	} else {
	    header << "HTTP/1.0 500 Server error\r\n";
	    header << "Content-Type: text/html\r\n";
	    header << "\r\n";
	    header << "<h1>unknown module: " << tmp << "</h1>\r\n";
	}
    } else if (strcmp(req, "/")) {
	std::string pn = req;
	a2pdf(httpd_root_ino, pn.c_str(), ut, ug, header);
    } else {
	header << "HTTP/1.0 500 Server error\r\n";
	header << "Content-Type: text/html\r\n";
	header << "\r\n";
	header << "<h1>unknown request: " << req << "</h1>\r\n";
    }

    std::string reply = header.str();
    tc->write(reply.data(), reply.size());
}

static str
read_file(const char *pn)
{
    fs_inode ino;
    error_check(fs_namei(pn, &ino));

    uint64_t len;
    error_check(fs_getsize(ino, &len));

    mstr s(len);
    error_check(fs_pread(ino, (void *) s.cstr(), len, 0));

    return s;
}

static void
do_login(const char *user, const char *pass, uint64_t *ug, uint64_t *ut)
{
    if (httpd_dj_enable) {
	dj_message_endpoint auth_ep;
	auth_ep.set_type(EP_GATE);
	auth_ep.ep_gate->msg_ct = dj_server_ct;
	auth_ep.ep_gate->gate = dj_authgate;

	/*
	 * Allocate a call taint category to protect the password in transit
	 */
	dj_calltaint = handle_alloc();
	label call_ct_label(1);
	call_ct_label.set(dj_calltaint, 3);
	int64_t call_ct = sys_container_alloc(start_env->shared_container,
					      call_ct_label.to_ulabel(),
					      "httpd dj call", 0, CT_QUOTA_INF);
	error_check(call_ct);

	/* XXX assumes publicly-writable container on the other side */
	dj_cat_mapping local_call_taint;
	dj_cat_mapping remote_call_taint;
	dj_stmt_signed delegation;

	label grant_local(3), grant_remote(3);
	dj_map_and_delegate(dj_calltaint, false,
			    grant_local, grant_remote,
			    call_ct, dj_server_ct, dj_server_pk,
			    the_gs, djcache,
			    &local_call_taint, &remote_call_taint, &delegation);

	/*
	 * Invoke the user auth code..
	 */
	authproxy_arg ap_arg;
	authproxy_res ap_res;

	ap_arg.username = user;
	ap_arg.password = pass;
	ap_arg.map_ct = dj_server_ct;
	ap_arg.return_map_ct = call_ct;

	label taint(1);
	taint.set(dj_calltaint, 3);

	label grant(3);
	grant.set(dj_calltaint, LB_LEVEL_STAR);

	dj_autorpc arpc(the_gs, 5, dj_server_pk, djcache);
	dj_delivery_code c = arpc.call(auth_ep, ap_arg, ap_res, &taint, &grant);
	if (c != DELIVERY_DONE)
	    throw basic_exception("auth rpc: code %d", c);
	if (!ap_res.ok)
	    throw basic_exception("authproxy: not ok");

	*ug = ap_res.resok->ug_remote.lcat;
	*ut = ap_res.resok->ut_remote.lcat;

	dj_ug = ap_res.resok->ug_remote.gcat;
	dj_ut = ap_res.resok->ut_remote.gcat;
    } else {
	auth_login(user, pass, ug, ut);
    }
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
		    do_login(user, pass, &ug, &ut);
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
    if (ac < 4) {
	cprintf("usage: %s bipipe-container bipipe-object auth-enable\n", av[0]);
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
    
    http_auth_enable = av[3][0] != '0' ? 1 : 0;
    httpd_root_ino = start_env->fs_root;
    
    if (dbg) {
	printf("httpd2: config:\n");
	printf(" %-20s %ld.%ld\n", "bipipe_seg", c, o);
	printf(" %-20s %d\n", "http_auth_enable", http_auth_enable);
    }

    if (httpd_dj_enable) {
	the_gs = new gate_sender();

	str dj_server_str = read_file("/dj_host");
	ptr<sfspub> sfspub = sfscrypt.alloc(dj_server_str,
					    SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	dj_server_pk = sfspub2dj(sfspub);

	dj_server_ct = atoi(read_file("/dj_ct").cstr());
	dj_authgate <<= read_file("/dj_authgate").cstr();
	dj_appgate <<= read_file("/dj_appgate").cstr();
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
