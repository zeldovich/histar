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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

#include <iostream>
#include <sstream>

static void __attribute__((noreturn))
httpd_worker(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    gcd->param_buf[sizeof(gcd->param_buf) - 1] = '\0';
    const char *user = &gcd->param_buf[0];
    uint32_t ulen = strlen(user);
    const char *req = "";
    if (ulen < sizeof(gcd->param_buf))
	req = user + ulen + 1;

    std::ostringstream out;

    out << "HTTP/2.0 200 OK\r\n";
    out << "Content-Type: text/html\r\n";
    out << "\r\n";
    out << "<h1>Hello " << user << "</h1>\r\n";
    out << "<p>\r\n";
    out << "Request path = " << req << "\r\n";
    out << "<p>\r\n";

    if (strcmp(req, "/")) {
	std::string pn = std::string("/home/") + user + req;
	int fd = open(pn.c_str(), O_RDONLY);
	if (fd < 0) {
	    out << "Cannot open " << pn << ": " << strerror(errno);
	} else {
	    char buf[4096];
	    for (;;) {
		int cc = read(fd, buf, sizeof(buf));
		if (cc < 0) {
		    out << "Cannot read " << pn << ": " << strerror(errno);
		    break;
		}
		if (cc == 0)
		    break;
		out.write(&buf[0], cc);
	    }
	}
    }

    std::string reply = out.str();
    struct cobj_ref reply_seg;
    void *va = 0;
    int r = segment_alloc(gcd->taint_container, reply.size(),
			  &reply_seg, &va, 0, "worker reply");
    if (r < 0) {
	cprintf("httpd_worker: cannot allocate reply: %s\n", e2s(r));
    } else {
	memcpy(va, reply.data(), reply.size());
	segment_unmap(va);
	gcd->param_obj = reply_seg;
    }

    gr->ret(0, 0, 0);
}

int
main(int ac, char **av)
{
    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);

    gate_create(start_env->shared_container, "worker",
		&th_ctm, &th_clr,
		&httpd_worker, 0);
    thread_halt();
}
