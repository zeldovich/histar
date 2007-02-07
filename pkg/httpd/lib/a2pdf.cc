extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
}

#include <iostream>
#include <sstream>


#include <inc/a2pdf.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/errno.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/spawn.hh>

static const char debug = 0;

uint64_t 
a2pdf(int fd, std::ostringstream &pdf_out, uint64_t utaint)
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

    int64_t otaint;
    error_check(otaint = handle_alloc());
    scope_guard<void , uint64_t> drop_otaint(thread_drop_star, otaint);
    
    label taint_label(0);
    if (utaint)
	taint_label.set(utaint, 3);
    taint_label.set(otaint, 3);

    error_check(fd_make_public(txt_in, taint_label.to_ulabel()));
    error_check(fd_make_public(errout, taint_label.to_ulabel()));
    error_check(fd_make_public(ps[0], taint_label.to_ulabel()));
    error_check(fd_make_public(ps[1], taint_label.to_ulabel()));
    error_check(fd_make_public(pdf[0], taint_label.to_ulabel()));
    error_check(fd_make_public(pdf[1], taint_label.to_ulabel()));

    label out;

    // Make a private /tmp for a2ps and gs
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

	const char *argv0[] = { pn0, "--output=-" };
	cp0 = spawn(start_env->proc_container, ino,
		    txt_in, ps[1], errout,
		    2, &argv0[0],
		    0, 0,
		    &cs, 0, 0, &dr, &taint_label,
		    0, fs_mtab_seg);
	close(ps[1]);

	error_check(fs_namei(pn1, &ino));
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
		    &cs, 0, 0, &dr, &taint_label,
		    0, fs_mtab_seg);
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
