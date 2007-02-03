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
#include <inc/spawn.hh>

#include <inc/errno.hh>

#include <iostream>
#include <sstream>

static struct fs_inode
fs_inode_for(const char *pn)
{
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	throw error(r, "cannot fs_lookup %s", pn);
    return ino;
}

uint64_t
a2pdf(int fd, std::ostringstream &out)
{
    const char *pn0 = "/bin/a2ps";
    const char *pn1 = "/bin/gs";

    struct child_process cp0, cp1;
    int64_t exit_code0, exit_code1;

    int txt_in = fd;
    
    int ps[2];
    errno_check(pipe(ps));
    int pdf[2];
    errno_check(pipe(pdf));

    try {
	struct fs_inode ino = fs_inode_for(pn0);
	const char *argv0[] = { pn0, "--output=-" };
	cp0 = spawn(start_env->root_container, ino,
		    txt_in, ps[1], 2,
		    2, &argv0[0],
		    0, 0,
		    0, 0, 0, 0, 0);
	close(ps[1]);

	ino = fs_inode_for(pn1);
	const char *argv1[] = { pn1, 
			       "-q",     
			       "-dNOPAUSE", 
			       "-dBATCH", 
			       "-sDEVICE=pdfwrite", 
			       "-sOutputFile=-", 
			       "-c",
			       ".setpdfwrite",
			       "-f",
			       "-" };
	
	cp1 = spawn(start_env->root_container, ino,
		    ps[0], pdf[1], 2,
		    10, &argv1[0],
		    0, 0,
		    0, 0, 0, 0, 0);
	close(ps[0]);
	close(pdf[1]);

    } catch (std::exception &e) {
	fprintf(stderr, "a2pdf_help: %s\n", e.what());
	throw e;
    }

    uint64_t ret = 0;
    for (;;) {
	char buf[512];
	int r;
	errno_check(r = read(pdf[0], buf, sizeof(buf)));
	if (r == 0)
	    break;
	out.write(buf, r);
	ret += r;
    }
    close(pdf[0]);
    
    error_check(process_wait(&cp0, &exit_code0));
    error_check(exit_code0);    

    error_check(process_wait(&cp1, &exit_code1));
    error_check(exit_code1);    
    
    return ret;
}

static void __attribute__((noreturn))
httpd_worker(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    gcd->param_buf[sizeof(gcd->param_buf) - 1] = '\0';
    const char *user = &gcd->param_buf[0];
    uint32_t ulen = strlen(user);
    const char *req = "";
    if (ulen < sizeof(gcd->param_buf))
	req = user + ulen + 1;

    std::ostringstream header;
    std::ostringstream pdf;

    if (strcmp(req, "/")) {
	std::string pn = std::string("/home/") + user + req;
	int fd = open(pn.c_str(), O_RDONLY);
	if (fd < 0) {
	    header << "Cannot open " << pn << ": " << strerror(errno);
	} else {
    
	    uint64_t sz = a2pdf(fd, pdf);

	    char size[32];
	    sprintf(size, "%ld", sz);
	    std::string content_length = std::string("Content-Length: ") + size + "\r\n";

	    header << "HTTP/2.0 200 OK\r\n";
	    header << "Content-Type: application/postscript\r\n";
	    header << content_length;
	    header << "\r\n";
	}
    }

    header << pdf.str();

    std::string reply = header.str();
    struct cobj_ref reply_seg;
    void *va = 0;

    cprintf("reply size %ld\n", reply.size());

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
