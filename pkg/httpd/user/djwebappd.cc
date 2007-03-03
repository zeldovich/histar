#include <sstream>
#include <inc/module.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>
#include <dj/miscx.h>

static void
handle_req(const fs_inode &httpd_root_ino, const char *req,
	   uint64_t ut, uint64_t ug, std::ostringstream &header)
{
    char *tmp;
    char strip_req[256];
    strncpy(strip_req, req, sizeof(strip_req) - 1);
    strip_req[sizeof(strip_req) - 1] = 0;

    if (tmp = strchr(strip_req, '?')) {
        *tmp = 0;
        tmp++;
        if (!strcmp(tmp, "a2pdf")) {
            std::string pn = strip_req;
            a2pdf(httpd_root_ino, pn.c_str(), ut, ug, header);
        } else if (!strcmp(tmp, "cat")) {
            std::string pn = strip_req;
            webcat(httpd_root_ino, pn.c_str(), ut, ug, header);
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
}

static bool
webapp_service(const dj_message &m, const str &s, dj_rpc_reply *r)
try {
    static fs_inode httpd_root_ino;
    if (!httpd_root_ino.obj.object)
	error_check(fs_namei("/www", &httpd_root_ino));

    webapp_arg arg;
    webapp_res res;

    if (!str2xdr(arg, s)) {
	warn << "webapp_service: cannot unmarshal\n";
	return false;
    }

    dj_catmap_indexed cmi(m.catmap);
    uint64_t ug, ut;
    if (!cmi.g2l(arg.ug, &ug) || !cmi.g2l(arg.ut, &ut)) {
	warn << "webapp_service: cannot map onto local categories\n";
	return false;
    }

    std::ostringstream obuf;
    handle_req(httpd_root_ino, arg.reqpath, ut, ug, obuf);

    std::string reply = obuf.str();
    res.httpres = str(reply.data(), reply.size());

    r->msg.msg = xdr2str(res);
    return true;
} catch (std::exception &e) {
    warn << "webapp_service: " << e.what() << "\n";
    return false;
}

static void
gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(webapp_service, gcd, r);
}

int
main(int ac, char **av)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djwebappd";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    warn << "djwebappd gate: " << g << "\n";
    thread_halt();
}
