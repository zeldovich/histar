extern "C" {
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <inc/perl.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/spawn.hh>

static const char debug = 1;

uint64_t
perl(const char *fn, std::ostringstream &perl_out, uint64_t utaint)
{
    const char *pn0 = "/bin/perl";

    struct child_process cp0;
    int64_t exit_code0;

    int sin, sout;
    int eout;
    int pout;

    // Check if script exists
    struct stat sb;
    if (stat(fn, &sb) < 0) {
	perl_out << "HTTP/1.0 404 Not Found\r\n";
	perl_out << "Content-Type: text/html\r\n";
	perl_out << "\r\n";
	perl_out << "Cannot find script " << fn << "\r\n";
	return 0;
    } 

    perl_out << "HTTP/2.0 200 OK\r\n";

    if (debug) {
	error_check(sin = opencons());
	eout = sin;
    } else {
	errno_check(sin = open("/dev/null", O_RDWR));
	eout = sin;
    }
    
    int p[2];
    errno_check(pipe(p));
    sout = p[1];
    pout = p[0];
    
    int64_t otaint;
    error_check(otaint = handle_alloc());
    scope_guard<void , uint64_t> drop_otaint(thread_drop_star, otaint);
    
    label taint_label(0);
    if (utaint)
	taint_label.set(utaint, 3);
    taint_label.set(otaint, 3);

    error_check(fd_make_public(sin, taint_label.to_ulabel()));
    error_check(fd_make_public(sout, taint_label.to_ulabel()));
    error_check(fd_make_public(eout, taint_label.to_ulabel()));
    error_check(fd_make_public(pout, taint_label.to_ulabel()));

    label out;

    // Make a private /tmp
    label tmp_label(1);
    tmp_label.merge(&taint_label, &out, label::max, label::leq_starlo);
    tmp_label.copy_from(&out);

    fs_inode self_dir;
    fs_get_root(start_env->shared_container, &self_dir);

    fs_inode tmp_dir;
    error_check(fs_mkdir(self_dir, "tmp", &tmp_dir, tmp_label.to_ulabel()));

    // Copy the mount table and mount our /tmp there
    int64_t new_mtab_id;
    error_check(new_mtab_id =
	sys_segment_copy(start_env->fs_mtab_seg, start_env->shared_container,
			 0, "wrap mtab"));
    cobj_ref fs_mtab_seg = COBJ(start_env->shared_container, new_mtab_id);
    fs_mount(fs_mtab_seg, start_env->fs_root, "tmp", tmp_dir);

    try {
	label cs(LB_LEVEL_STAR);
	cs.merge(&taint_label, &out, label::max, label::leq_starlo);
	cs.copy_from(&out);
	
	label dr(1);
	dr.merge(&taint_label, &out, label::max, label::leq_starlo);
	dr.copy_from(&out);

	dr.set(start_env->user_grant, 3);

	struct fs_inode ino;
	error_check(fs_namei(pn0, &ino));

	const char *argv0[] = { pn0, fn };
	cp0 = spawn(start_env->proc_container, ino,
		    sin, sout, eout,
		    2, &argv0[0],
		    0, 0,
		    &cs, 0, 0, &dr, &taint_label,
		    0, fs_mtab_seg);
	
	close(sin);
	close(sout);
	close(eout);
    } catch (std::exception &e) {
	cprintf("perl: %s\n", e.what());
	throw e;
    }

    uint64_t ret = 0;
    for (;;) {
	char buf[512];
	int r;
	errno_check(r = read(pout, buf, sizeof(buf)));
	if (r == 0)
	    break;
	perl_out.write(buf, r);
	ret += r;
    }
    close(pout);
    
    error_check(process_wait(&cp0, &exit_code0));
    error_check(exit_code0);    
    
    return ret;

}
