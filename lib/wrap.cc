extern "C" {
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <inc/wrap.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/spawn.hh>

static const char debug = 0;

wrap_call::wrap_call(const char *pn, fs_inode root_ino) :
    sin_(-1), eout_(-1), root_ino_(root_ino), called_(0)
{
    pn_ = strdup(pn);
    
    uint64_t ct = start_env->shared_container;
    int64_t obj;
    error_check(obj = sys_container_alloc(ct, 0, "wrap data", 0, CT_QUOTA_INF));
    call_ct_ = COBJ(ct, obj);

    int p[2];
    errno_check(::pipe(p));
    sout_ = p[1];
    wout_ = p[0];
}

wrap_call::~wrap_call(void)
{
    free(pn_);
    if (wout_ >= 0)
	close(wout_);
    if (sout_ >= 0)
	close(sout_);
    sys_obj_unref(call_ct_);
}

void
wrap_call::print_to(int fd, std::ostringstream &out)
{
    for (;;) {
	char buf[1024];
	int r;
	errno_check(r = read(fd, buf, sizeof(buf)));
	if (r == 0)
	    break;
	out.write(buf, r);
    }
    return;
}

void
wrap_call::pipe(wrap_call *wc ,int ac, const char **av, 
		int ec, const char **ev, label *taint_label)
{
    wc->sin_ = wout_;
    wc->call(ac, av, ec, ev, taint_label);
    
    close(wout_);
    wout_ = -1;
}

void
wrap_call::pipe(wrap_call *wc ,int ac, const char **av, 
		int ec, const char **ev, 
		label *taint_label, std::ostringstream &out)
{
    pipe(wc, ac, av, ec, ev, taint_label);
    print_to(wc->wout_, out);
}

void
wrap_call::call(int ac, const char **av, int ec, const char **ev, 
		label *taint)
{
    if (called_)
	throw basic_exception("wrap_call::call already called");
    called_ = 1;

    int def_fd = debug ? opencons() : open("/dev/null", O_RDWR);
    scope_guard<int, int> close_def(close, def_fd);

    if (sin_ < 0) 
	sin_ = def_fd;
    if (eout_ < 0)
	eout_ = def_fd;
           
    label taint_label(0);
    if (taint)
	taint_label = *taint;

    error_check(fd_make_public(sin_, taint_label.to_ulabel()));
    error_check(fd_make_public(sout_, taint_label.to_ulabel()));
    error_check(fd_make_public(eout_, taint_label.to_ulabel()));
    error_check(fd_make_public(wout_, taint_label.to_ulabel()));

    label tmp;

    // Make a private /tmp
    label tmp_label(1);
    tmp_label.merge(&taint_label, &tmp, label::max, label::leq_starlo);
    tmp_label = tmp;

    fs_inode self_dir;
    fs_get_root(call_ct_.object, &self_dir);

    fs_inode tmp_dir;
    error_check(fs_mkdir(self_dir, "tmp", &tmp_dir, tmp_label.to_ulabel()));

    // Copy the mount table and mount our /tmp there
    int64_t new_mtab_id;
    error_check(new_mtab_id =
	sys_segment_copy(start_env->fs_mtab_seg, call_ct_.object,
			 0, "wrap mtab"));
    cobj_ref fs_mtab_seg = COBJ(call_ct_.object, new_mtab_id);
    fs_mount(fs_mtab_seg, root_ino_, "tmp", tmp_dir);

    label cs(LB_LEVEL_STAR);
    cs.merge(&taint_label, &tmp, label::max, label::leq_starlo);
    cs = tmp;
    
    label dr(1);
    dr.merge(&taint_label, &tmp, label::max, label::leq_starlo);
    dr = tmp;
    
    dr.set(start_env->user_grant, 3);
    
    struct fs_inode ino;
    error_check(fs_namei(pn_, &ino));

    spawn_descriptor sd;
    sd.ct_ = call_ct_.object;
    sd.elf_ino_ = ino;
    
    sd.fd0_ = sin_;
    sd.fd1_ = sout_;
    sd.fd2_ = eout_;

    sd.ac_ = ac;
    sd.av_ = av;
    sd.envc_ = ec;
    sd.envv_ = ev;
    
    sd.cs_ = &cs;
    sd.dr_ = &dr;
    sd.co_ = &taint_label;
    
    sd.fs_mtab_seg_ = fs_mtab_seg;
    sd.fs_root_ = root_ino_;
    sd.fs_cwd_ = root_ino_;

    cp_ = spawn(&sd);

    close(sout_);
    sout_ = -1;
}

void
wrap_call::call(int ac, const char **av, int ec, const char **ev, 
		label *taint, std::ostringstream &out)
{
    call(ac, av, ec, ev, taint);
    print_to(wout_, out);
}
