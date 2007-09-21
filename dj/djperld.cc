extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
}

#include <async.h>
#include <inc/labelutil.hh>
#include <inc/errno.hh>
#include <inc/spawn.hh>
#include <dj/djgatesrv.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/hsutil.hh>
#include <dj/miscx.h>

static int
run_perl(str script, str input, str *output)
{
    int64_t new_mtab_sg;
    error_check(new_mtab_sg =
	sys_segment_copy(start_env->fs_mtab_seg,
			 start_env->shared_container,
			 0, "mtab"));

    start_env->fs_mtab_seg = COBJ(start_env->shared_container, new_mtab_sg);

    fs_inode self_dir;
    fs_get_root(start_env->shared_container, &self_dir);

    fs_inode input_ino, script_ino;
    error_check(fs_create(self_dir, "input", &input_ino, 0));
    error_check(fs_create(self_dir, "script", &script_ino, 0));

    fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "input", input_ino);
    fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "script", script_ino);

    error_check(fs_pwrite(input_ino, input.cstr(), input.len(), 0));
    error_check(fs_pwrite(script_ino, script.cstr(), script.len(), 0));

    int fds[2];
    errno_check(pipe(fds));

    fs_inode perl_ino;
    error_check(fs_namei("/bin/perl", &perl_ino));

    int nullfd = open("/dev/null", O_RDWR);
    if (nullfd < 0)
	throw basic_exception("cannot open /dev/null: %s", strerror(errno));

    error_check(fd_make_public(nullfd, 0));
    error_check(fd_make_public(fds[1], 0));

    const char *argv[] = { "perl", "-f", "/script", "/input" };

    spawn_descriptor sd;
    sd.ct_ = start_env->shared_container;
    sd.elf_ino_ = perl_ino;
    sd.fd0_ = nullfd;
    sd.fd1_ = fds[1];
    sd.fd2_ = fds[1];

    label tl;
    thread_cur_label(&tl);
    tl.transform(label::star_to, tl.get_default());

    sd.cs_ = &tl;
    sd.dr_ = &tl;
    sd.co_ = &tl;

    sd.ac_ = 4;
    sd.av_ = argv;
    sd.envc_ = 0;
    sd.envv_ = 0;
    sd.spawn_flags_ = SPAWN_NO_AUTOGRANT;

    child_process cp = spawn(&sd);
    close(fds[1]);

    strbuf out;
    for (;;) {
	char buf[1024];
	ssize_t cc = read(fds[0], &buf[0], sizeof(buf));
	if (cc < 0)
	    throw basic_exception("Error reading from pipe: %s", strerror(errno));
	if (cc == 0)
	    break;
	out << str(&buf[0], cc);
    }

    int64_t exit_code;
    error_check(process_wait(&cp, &exit_code));

    *output = out;
    return exit_code;
}

bool
dj_perl_service(const dj_message &m, const str &s, dj_rpc_reply *r)
{
    try {
	perl_run_arg parg;
	perl_run_res pres;

	if (!str2xdr(parg, s))
	    throw basic_exception("cannot unmarshal");

	str input = parg.input;
	str script = parg.script;
	str output;
	int ec = run_perl(script, input, &output);
	pres.retval = ec;
	pres.output = output;

	r->msg.msg = xdr2str(pres);
	return true;
    } catch (std::exception &e) {
	cprintf("djperld: %s\n", e.what());
	return false;
    }
}

static void
gate_entry(uint64_t arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(dj_perl_service, gcd, r);
}

int
main(int ac, char **av)
{
    // We need to do this not-so-amusing dance to call fd_handles_init()
    // which can touch a bad file descriptor mapping in the tainted AS.
    int pipefd[2];
    if (pipe(pipefd) < 0) {
	warn << "cannot create pipes?\n";
	exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    label lpub(1);

    int64_t call_ct;
    error_check(call_ct = sys_container_alloc(start_env->shared_container,
					      lpub.to_ulabel(), "public call",
					      0, CT_QUOTA_INF));

    warn << "djperld public container: " << call_ct << "\n";

    label srv_label;
    thread_cur_label(&srv_label);
    srv_label.set(start_env->process_grant, 1);

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djperld";
    gd.func_ = &gate_entry;
    gd.label_ = &srv_label;

    cobj_ref g = gate_create(&gd);
    warn << "djperld gate: " << g << "\n";

    //str script = "print 'A'x5; print <>;";
    //str input = "Hello world\n";
    //str output;

    //int ec = run_perl(script, input, &output);
    //warn << "run_perl: ec " << ec << "\n";
    //warn << "output: " << output << "\n";

    thread_halt();
}
