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
#include <inc/debug.h>

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

static char debug = 0;

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
a2pdf(int fd, std::ostringstream &pdf_out)
{
    const char *pn0 = "/bin/a2ps";
    const char *pn1 = "/bin/gs";

    struct child_process cp0, cp1;
    int64_t exit_code0, exit_code1;

    int txt_in = fd;
    int errout;
    if (debug)
	error_check(errout = opencons());
    else
	errno_check(errout = open("/dev/null", O_RDWR));
    
    int ps[2];
    errno_check(pipe(ps));
    int pdf[2];
    errno_check(pipe(pdf));

    label taint_label(1);
    label tmp(1), out;
    thread_cur_label(&taint_label);
    taint_label.merge(&tmp, &out, label::max, label::leq_starlo);
    taint_label.copy_from(&out);
    
    // create a tainted /tmp for a2ps and gs
    label mtab_label(1);
    mtab_label.copy_from(&taint_label);

    int64_t base_ct;
    error_check(base_ct = sys_container_alloc(start_env->proc_container, 
					      mtab_label.to_ulabel(), "scratch",
					      0, CT_QUOTA_INF));
    scope_guard<int, struct cobj_ref> 
	unref_base(sys_obj_unref, COBJ(start_env->proc_container, base_ct));

    int64_t mtab_id;
    error_check(mtab_id = sys_segment_copy(start_env->fs_mtab_seg, 
				     base_ct, mtab_label.to_ulabel(), "mtab"));
    start_env->fs_mtab_seg = COBJ(base_ct, mtab_id);

    fs_inode root_ino, tmp_ino, scratch_ino;
    error_check(fs_namei("/", &root_ino));
    fs_get_root(base_ct, &scratch_ino);
    error_check(fs_mkdir(scratch_ino, "tmp", &tmp_ino, mtab_label.to_ulabel()));
    error_check(fs_mount(start_env->fs_mtab_seg, root_ino, "tmp", tmp_ino));

    try {
	label cs(LB_LEVEL_STAR);
	cs.merge(&taint_label, &out, label::max, label::leq_starlo);
	cs.copy_from(&out);

	label ds(3);
	ds.set(start_env->user_grant, LB_LEVEL_STAR);

	label dr(1);
	dr.merge(&taint_label, &out, label::max, label::leq_starlo);
	dr.copy_from(&out);
	dr.set(start_env->user_grant, 3);

	struct fs_inode ino = fs_inode_for(pn0);
	const char *argv0[] = { pn0, "--output=-" };
	cp0 = spawn(start_env->proc_container, ino,
		    txt_in, ps[1], errout,
		    2, &argv0[0],
		    0, 0,
		    &cs, &ds, 0, &dr, 0, SPAWN_NO_AUTOGRANT);
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
	
	cp1 = spawn(start_env->proc_container, ino,
		    ps[0], pdf[1], errout,
		    10, &argv1[0],
		    0, 0,
		    &cs, &ds, 0, &dr, 0, SPAWN_NO_AUTOGRANT);
	close(ps[0]);
	close(pdf[1]);
	close(errout);
    } catch (std::exception &e) {
	cprintf("a2pdf_help: %s\n", e.what());
	throw e;
    }

    uint64_t ret = 0;
    for (;;) {
	char buf[512];
	int r;
	errno_check(r = read(pdf[0], buf, sizeof(buf)));
	if (r == 0)
	    break;
	pdf_out.write(buf, r);
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

	    header << "HTTP/1.0 200 OK\r\n";
	    header << "Content-Type: application/pdf\r\n";
	    header << content_length;
	    header << "\r\n";
	}
    }

    header << pdf.str();

    std::string reply = header.str();
    struct cobj_ref reply_seg;
    void *va = 0;

    debug_print(debug, "reply size %ld\n", reply.size());

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
    gate_create(start_env->shared_container, "worker",
		0, 0, 0, &httpd_worker, 0);
    thread_halt();
}
