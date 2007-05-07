extern "C" {
#include <inc/syscall.h>
}

#include <sstream>
#include <inc/module.hh>
#include <inc/labelutil.hh>
#include <dj/gatesender.hh>
#include <dj/djcache.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>
#include <dj/djautorpc.hh>
#include <dj/hsutil.hh>
#include <dj/miscx.h>
#include <dj/djfs.h>

enum { webapp_debug = 0 };

static gate_sender *the_gs;
static fs_inode httpd_root_ino;

static void
handle_req(const char *req, uint64_t ut, uint64_t ug,
	   std::ostringstream &header,
	   dj_global_cache &cache, const dj_pubkey &userhost,
	   const dj_message_endpoint &user_fs)
{
    char *tmp;
    char strip_req[256];
    strncpy(strip_req, req, sizeof(strip_req) - 1);
    strip_req[sizeof(strip_req) - 1] = 0;

    start_env->user_grant = ug;
    start_env->user_taint = ut;

    if ((tmp = strchr(strip_req, '?'))) {
        *tmp = 0;
        tmp++;

	/* Fetch requested file */
	djfs_request arg;
	djfs_reply res;

	arg.set_op(DJFS_READ);
	arg.read->pn = strip_req;

	label taint(1), grant(3), clear(0);
	taint.set(ut, 3);
	grant.set(ug, LB_LEVEL_STAR);
	clear.set(ut, 3);

	dj_autorpc fs_arpc(the_gs, 5, userhost, cache);
	dj_delivery_code c = fs_arpc.call(user_fs, arg, res, &taint, &grant, &clear);
	if (c != DELIVERY_DONE || res.err) {
	    header << "HTTP/1.0 500 Server error\r\n";
	    header << "Content-Type: text/html\r\n";
	    header << "\r\n";
	    header << "FS error: code " << c << ", err " << res.err << "\r\n";
	    return;
	}

	/* Create a mount table to stick this file in our namespace */
	str filedata = str(res.d->read->data.base(), res.d->read->data.size());
	cobj_ref file_obj = str_to_segment(start_env->shared_container,
					   filedata, "blah");

	int64_t mtab_id = sys_segment_copy(start_env->fs_mtab_seg,
					   start_env->shared_container,
					   0, "new mtab");
	error_check(mtab_id);
	start_env->fs_mtab_seg = COBJ(start_env->shared_container, mtab_id);

	fs_inode file_ino;
	file_ino.obj = file_obj;
	error_check(fs_mount(start_env->fs_mtab_seg, start_env->fs_root,
			     "infile", file_ino));
	std::string pn = "/infile";

        if (!strcmp(tmp, "a2pdf")) {
            a2pdf(httpd_root_ino, pn.c_str(), ut, ug, header);
        } else if (!strcmp(tmp, "cat")) {
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
    webapp_arg arg;
    webapp_res res;

    if (!str2xdr(arg, s)) {
	warn << "webapp_service: cannot unmarshal\n";
	return false;
    }

    uint64_t ug = arg.ug_map_apphost.lcat;
    uint64_t ut = arg.ut_map_apphost.lcat;

    if (webapp_debug) {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);
	warn << "webapp_service: ug " << ug << ", ut " << ut << "\n";
	warn << "tl = " << tl.to_string() << "\n";
	warn << "tc = " << tc.to_string() << "\n";
    }

    dj_global_cache djcache;
    dj_pubkey thiskey = the_gs->hostkey();
    djcache.dmap_.insert(arg.ug_dlg_apphost);
    djcache.dmap_.insert(arg.ut_dlg_apphost);
    djcache.dmap_.insert(arg.ug_dlg_userhost);
    djcache.dmap_.insert(arg.ut_dlg_userhost);

    djcache[thiskey]->cmi_.insert(arg.ug_map_apphost);
    djcache[thiskey]->cmi_.insert(arg.ut_map_apphost);
    djcache[arg.userhost]->cmi_.insert(arg.ug_map_userhost);
    djcache[arg.userhost]->cmi_.insert(arg.ut_map_userhost);

    std::ostringstream obuf;
    handle_req(arg.reqpath, ut, ug, obuf,
	       djcache, arg.userhost, arg.user_fs);

    std::string reply = obuf.str();
    res.httpres = str(reply.data(), reply.size());

    r->msg.msg = xdr2str(res);
    return true;
} catch (std::exception &e) {
    warn << "webapp_service: " << e.what() << "\n";
    return false;
}

static void
gate_entry(uint64_t arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(webapp_service, gcd, r);
}

int
main(int ac, char **av)
{
    // We need to do this not-so-amusing dance to call fd_handles_init()
    // which can touch a bad file descriptor mapping in the tainted AS.
    int pipefd[2];
    if (pipe(pipefd) < 0)
	fatal << "cannot create pipes?\n";

    close(pipefd[0]);
    close(pipefd[1]);

    for (int retry = 0; ; retry++) {
	int r = fs_namei("/www", &httpd_root_ino);
	if (r >= 0)
	    break;

	if (retry >= 100) {
	    printf("djwebappd: tired of waiting for /www to appear\n");
	    exit(1);
	}
	sleep(1);
    }

    for (int retry = 0; ; retry++) {
	try {
	    the_gs = new gate_sender();
	    break;
	} catch (...) {
	    if (retry >= 10)
		throw;
	    sleep(1);
	}
    }

    int64_t mtab_id = sys_segment_copy(start_env->fs_mtab_seg,
				       start_env->shared_container,
				       0, "djwebappd mtab");
    error_check(mtab_id);
    start_env->fs_mtab_seg = COBJ(start_env->shared_container, mtab_id);

    /* Replicate the mounted directory structure that inetd creates for /www */
    fs_inode home_dir, etc_dir, share_dir, bin_dir, dev_dir;
    error_check(fs_namei("/home", &home_dir));
    error_check(fs_namei("/etc", &etc_dir));
    error_check(fs_namei("/share", &share_dir));
    error_check(fs_namei("/bin", &bin_dir));
    error_check(fs_namei("/dev", &dev_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, httpd_root_ino, "home", home_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, httpd_root_ino, "etc", etc_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, httpd_root_ino, "share", share_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, httpd_root_ino, "bin", bin_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, httpd_root_ino, "dev", dev_dir));
    start_env->fs_root = httpd_root_ino;

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djwebappd";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    warn << "djwebappd gate: " << g << "\n";
    thread_halt();
}
