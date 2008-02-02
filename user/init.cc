extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/gateparam.h>
#include <inc/authd.h>
#include <inc/time.h>
#include <inc/pty.h>
#include <inc/error.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>

#define SPAWN_ROOT_CT		0x01
#define SPAWN_WAIT_GC		0x02
#define SPAWN_OTHER_CT		0x04

static int init_debug = 0;
static const char *env[] = { "USER=root", "HOME=/" };
static uint64_t time_grant;

/*
 * Glue to make init link properly as a static binary even though
 * the rest of the system uses shared libraries.
 */
extern "C" void _dl_app_init_array(void) {}
extern "C" void _dl_app_fini_array(void) {}
extern "C" int dl_iterate_phdr(void) { return 0; }

static struct child_process
spawn_fs(int flags, int fd, const char *pn, const char *arg,
	 label *ds, label *dr, uint64_t ct = 0)
{
    struct child_process cp;
    try {
	struct fs_inode ino;
	int r = fs_namei(pn, &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup %s", pn);

	const char *argv[] = { pn, arg };
	uint64_t pct = (flags & SPAWN_ROOT_CT)  ? start_env->root_container :
		       (flags & SPAWN_OTHER_CT) ? ct
						: start_env->process_pool;
	cp = spawn(pct,
		   ino, fd, fd, fd,
	           arg ? 2 : 1, &argv[0],
		   sizeof(env)/sizeof(env[0]), &env[0],
	           0, ds, 0, dr, 0, SPAWN_NO_AUTOGRANT);

	if (init_debug)
	    printf("init: spawned %s, ds = %s\n", pn, ds->to_string());

	if (flags & SPAWN_WAIT_GC) {
	    int64_t code;
	    r = process_wait(&cp, &code);
	    if (r == PROCESS_EXITED)
		sys_obj_unref(COBJ(pct, cp.container));
	}
    } catch (std::exception &e) {
	cprintf("spawn_fs(%s): %s\n", pn, e.what());
    }
    return cp;
}

static void
init_env(uint64_t c_root, uint64_t c_self, uint64_t h_root)
{
    start_env = 0;
    struct cobj_ref sa;
    error_check(segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env,
			      0, "init env"));

    void *start_env_ro = (void *) USTARTENVRO;
    error_check(segment_map(sa, 0, SEGMAP_READ, &start_env_ro, 0, 0));

    start_env->proc_container = c_self;
    start_env->shared_container = c_self;
    start_env->root_container = c_root;
    start_env->process_grant = h_root;
    start_env->process_taint = handle_alloc();

    start_env->user_grant = h_root;
    start_env->user_taint = 0;

    error_check(segment_alloc(c_self, sizeof(struct fs_mount_table),
			      &start_env->fs_mtab_seg, 0, 0, "mount table"));

    time_grant = handle_alloc();
    label time_label(1);
    time_label.set(time_grant, 0);

    struct time_of_day_seg *tods = 0;
    error_check(segment_alloc(c_self, sizeof(struct time_of_day_seg),
			      &start_env->time_seg, (void **) &tods,
			      time_label.to_ulabel(), "time-of-day"));
    tods->unix_nsec_offset = NSEC_PER_SECOND * UINT64(1000000000);
}

static void
init_fs(int cons)
{
    uint64_t c_root = start_env->root_container;

    int64_t old_fsroot = container_find(c_root, kobj_container, "fsroot");
    if (old_fsroot >= 0) {
	start_env->fs_root.obj = COBJ(c_root, old_fsroot);
    } else {
	struct fs_inode root_ct_dir;
	fs_get_root(c_root, &root_ct_dir);
	error_check(fs_mkdir(root_ct_dir, "fsroot", &start_env->fs_root, 0));
    }

    // start out in the root directory
    start_env->fs_cwd = start_env->fs_root;

    // mount the root container on /rct
    struct fs_inode xino;
    fs_get_root(start_env->root_container, &xino);
    error_check(fs_mount(start_env->fs_mtab_seg,
			 start_env->fs_root, "rct", xino));

    // create a process pool
    int64_t procpool = sys_container_alloc(start_env->root_container, 0,
					   "procpool", 0, CT_QUOTA_INF);
    error_check(procpool);
    start_env->process_pool = procpool;

    // no need to initialize everything again if it's already there
    if (old_fsroot >= 0)
	return;

    // hard-link the binaries over (to have a directory segment)
    int64_t embed_bin_id = container_find(c_root, kobj_container, "embed_bins");
    if (embed_bin_id < 0)
	throw error(embed_bin_id, "cannot find embed_bins");

    struct fs_inode bin_dir;
    error_check(fs_mkdir(start_env->fs_root, "bin", &bin_dir, 0));

    uint64_t ct_slot = 0;
    for (;;) {
	int64_t id = sys_container_get_slot_id(embed_bin_id, ct_slot++);
	if (id == -E_NOT_FOUND)
	    continue;
	if (id < 0)
	    break;

	struct fs_inode file_ino;
	file_ino.obj = COBJ(embed_bin_id, id);

	char name[KOBJ_NAME_LEN];
	error_check(sys_obj_get_name(file_ino.obj, &name[0]));
	error_check(sys_obj_set_fixedquota(file_ino.obj));
	error_check(fs_link(bin_dir, name, file_ino, 0));
    }

    // symlink "sh" to "ksh"
    unlink("/bin/sh");
    symlink("ksh", "/bin/sh");

    // create symlinks from "ls" to "jls", etc, as needed
    if (access("/bin/ls", F_OK) < 0)
	symlink("jls", "/bin/ls");
    if (access("/bin/cp", F_OK) < 0)
	symlink("jcp", "/bin/cp");
    if (access("/bin/rm", F_OK) < 0)
	symlink("jrm", "/bin/rm");
    if (access("/bin/mkdir", F_OK) < 0)
	symlink("jmkdir", "/bin/mkdir");
    if (access("/bin/sync", F_OK) < 0)
	symlink("jsync", "/bin/sync");
    if (access("/bin/cat", F_OK) < 0)
	symlink("jcat", "/bin/cat");
    if (access("/bin/echo", F_OK) < 0)
	symlink("jecho", "/bin/echo");
    if (access("/bin/true", F_OK) < 0)
	symlink("jtrue", "/bin/true");
    if (access("/bin/date", F_OK) < 0)
	symlink("jdate", "/bin/date");

    // create a /dev directory
    struct fs_inode dev;
    error_check(fs_mkdir(start_env->fs_root, "dev", &dev, 0));

    struct fs_inode dummy_ino;
    error_check(fs_mknod(dev, "null", devnull.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "zero", devzero.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "console", devcons.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "fb0", devfb.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "tty", devtty.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "random", devrand.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "urandom", devrand.dev_id, 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "ptmx", devptm.dev_id, 0, &dummy_ino, 0));

    // create a /home directory
    struct fs_inode fs_home;
    error_check(fs_mkdir(start_env->fs_root, "home", &fs_home, 0));

    // create a /share directory
    error_check(fs_mkdir(start_env->fs_root, "share", &dummy_ino, 0));

    // create a /proc directory
    error_check(fs_mkdir(start_env->fs_root, "proc", &dummy_ino, 0));
    symlink("/netd/proc/net", "/proc/net");

    // create /tmp
    label ltmp(1);
    error_check(fs_mkdir(start_env->fs_root, "tmp", &dummy_ino, ltmp.to_ulabel()));

    struct fs_inode etc;
    error_check(fs_mkdir(start_env->fs_root, "etc", &etc, 0));

    symlink("/netd/resolv.conf", "/etc/resolv.conf");

    struct fs_inode hosts, passwd, group, consfont;
    error_check(fs_create(etc, "hosts", &hosts, 0));
    error_check(fs_create(etc, "passwd", &passwd, 0));
    error_check(fs_create(etc, "group", &group, 0));
    error_check(fs_create(etc, "consfont", &consfont, 0));

    const char *hosts_data = "171.66.3.9 www.scs.stanford.edu www\n";
    error_check(fs_pwrite(hosts, hosts_data, strlen(hosts_data), 0));

    const char *passwd_data =
	"root:x:0:0:root:/:/bin/ksh\n"
	"ftp:x:14:50:FTP User:/var/ftp:/sbin/nologin\n"
	"clamav:x:91:91:clamav user:/:/bin/ksh\n";
    error_check(fs_pwrite(passwd, passwd_data, strlen(passwd_data), 0));

    const char *group_data =
	"root:x:0:\n";
    error_check(fs_pwrite(group, group_data, strlen(group_data), 0));

    const char *consfont_data = "Monospace-20\n";
    error_check(fs_pwrite(consfont, consfont_data, strlen(consfont_data), 0));

    // finish more FS initialization in a shell script
    label root_ds(3);
    root_ds.set(start_env->user_grant, LB_LEVEL_STAR);
    root_ds.set(start_env->user_taint, LB_LEVEL_STAR);

    label root_dr(0);
    root_dr.set(start_env->user_grant, 3);
    root_dr.set(start_env->user_taint, 3);

    spawn_fs(SPAWN_WAIT_GC, cons,
	     "/bin/ksh", "/bin/init.sh",
	     &root_ds, &root_dr);
}

static void
init_auth(int cons, const char *shroot)
{
    // create, mount uauth
    int64_t uauth_id;
    int64_t old_uauth = container_find(start_env->root_container,
				       kobj_container, "uauth");
    if (old_uauth >= 0) {
	uauth_id = old_uauth;
    } else {
	uauth_id = sys_container_alloc(start_env->root_container, 0, "uauth",
				       0, CT_QUOTA_INF);
	error_check(uauth_id);
    }

    struct fs_inode xino;
    fs_get_root(uauth_id, &xino);
    error_check(fs_mount(start_env->fs_mtab_seg,
			 start_env->fs_root, "uauth", xino));

    if (old_uauth >= 0)
	return;

    struct child_process cp;
    int64_t ec;

    // spawn user-auth agent for root
    fs_inode uauth_dir;
    error_check(fs_namei("/uauth", &uauth_dir));

    cp = spawn_fs(SPAWN_OTHER_CT, cons, "/bin/auth_log", 0, 0, 0,
		  uauth_dir.obj.object);
    error_check(process_wait(&cp, &ec));

    cp = spawn_fs(SPAWN_OTHER_CT, cons, "/bin/auth_dir", shroot, 0, 0,
		  uauth_dir.obj.object);
    error_check(process_wait(&cp, &ec));

    fs_inode user_authd;
    error_check(fs_namei("/bin/auth_user", &user_authd));

    char root_grant[32], root_taint[32];
    sprintf(&root_grant[0], "%"PRIu64, start_env->user_grant);
    sprintf(&root_taint[0], "%"PRIu64, start_env->user_taint);
    const char *argv[] = { "auth_user", root_grant, root_grant, root_taint };

    struct spawn_descriptor sd;
    sd.ct_ = uauth_dir.obj.object;
    sd.ctname_ = "root";
    sd.elf_ino_ = user_authd;
    sd.fd0_ = cons;
    sd.fd1_ = cons;
    sd.fd2_ = cons;
    sd.ac_ = sizeof(argv)/sizeof(argv[0]);
    sd.av_ = argv;
    sd.spawn_flags_ = SPAWN_COPY_MTAB;
    cp = spawn(&sd);
    error_check(process_wait(&cp, &ec));

    // register this user-agent with the auth directory
    int64_t uauth_gate;
    error_check(uauth_gate =
	container_find(cp.container, kobj_gate, "user login gate"));

    fs_inode auth_dir_gt;
    error_check(fs_namei_flags("/uauth/auth_dir/authdir", &auth_dir_gt,
			       NAMEI_LEAF_NOEVAL));

    gate_call_data gcd;
    auth_dir_req   *req   = (auth_dir_req *)   &gcd.param_buf[0];
    auth_dir_reply *reply = (auth_dir_reply *) &gcd.param_buf[0];
    req->op = auth_dir_add;
    strcpy(&req->user[0], "root");
    req->user_gate = COBJ(cp.container, uauth_gate);

    label verify(3);
    verify.set(start_env->user_grant, 0);
    gate_call(auth_dir_gt.obj, 0, 0, 0).call(&gcd, &verify);
    error_check(reply->err);
    auth_chpass("root", "", "r");
}

static void
init_procs(int cons)
{
    label root_ds(3);
    root_ds.set(start_env->user_grant, LB_LEVEL_STAR);
    root_ds.set(start_env->user_taint, LB_LEVEL_STAR);

    label root_dr(0);
    root_dr.set(start_env->user_grant, 3);
    root_dr.set(start_env->user_taint, 3);

    char root_grant_buf[32];
    snprintf(&root_grant_buf[0], sizeof(root_grant_buf), "%"PRIu64,
	     start_env->user_grant);

    try {
	init_auth(cons, &root_grant_buf[0]);
    } catch (std::exception &e) {
	printf("init_procs: cannot init auth system: %s\n", e.what());
    }

    label time_ds(3), time_dr(0);
    time_ds.set(time_grant, LB_LEVEL_STAR);
    time_dr.set(time_grant, 3);
    spawn_fs(0, cons, "/bin/jntpd", "pool.ntp.org", &time_ds, &time_dr);

    FILE *inittab = fopen("/bin/inittab", "r");
    if (inittab) {
	char buf[256];
	while (fgets(buf, sizeof(buf), inittab)) {
	    char *fn = &buf[0];
	    char *priv = fn;
	    strsep(&priv, ":");
	    if (!priv) {
		cprintf("init_procs: bad entry? %s\n", buf);
		continue;
	    }

	    char *flag_root = strchr(priv, 'r');
	    char *flag_args = strchr(priv, 'a');
	    char *flag_ct   = strchr(priv, 'c');

	    spawn_fs(flag_ct ? SPAWN_ROOT_CT : 0, cons, fn,
		     flag_args ? &root_grant_buf[0] : 0,
		     flag_root ? &root_ds : 0,
		     flag_root ? &root_dr : 0);
	}
    }
}

static void __attribute__((noreturn))
run_shell(int cons)
{
    int r;
    label ds(3);
    ds.set(start_env->user_grant, LB_LEVEL_STAR);
    ds.set(start_env->user_taint, LB_LEVEL_STAR);

    label dr(0);
    dr.set(start_env->user_grant, 3);
    dr.set(start_env->user_taint, 3);

    for (;;) {
        struct child_process shell_proc = spawn_fs(0, cons, "/bin/ksh", "-l", &ds, &dr);
        int64_t exit_code = 0;
        if ((r = process_wait(&shell_proc, &exit_code)) < 0)
            cprintf("run_shell: process_wait error: %s\n", e2s(r));
        else
            cprintf("run_shell: shell exit value %"PRId64"\n", exit_code);
	sys_obj_unref(COBJ(start_env->process_pool, shell_proc.container));
    }
}

static void __attribute__((noreturn))
run_shell_entry(void *arg)
{
    int cons = (uintptr_t) arg;
    run_shell(cons);
}

static int
init_fbcons(int basecons, int *consp, int maxvt)
{
    consp[0] = basecons;

    /* Check if we are running in a graphical console */
    struct jos_fb_mode fb;
    if (sys_fb_get_mode(&fb) < 0)
	return 1;

    int64_t fbc_grant = handle_alloc();
    int64_t fbc_taint = handle_alloc();

    char a0[64], a1[64];
    sprintf(a0, "%"PRIu64, fbc_grant);
    sprintf(a1, "%"PRIu64, fbc_taint);

    fs_inode ino;
    if (fs_namei("/bin/fbconsd", &ino) < 0)
	return 1;

    label ds(3), dr(0);
    ds.set(fbc_grant, LB_LEVEL_STAR);
    ds.set(fbc_taint, LB_LEVEL_STAR);
    dr.set(fbc_grant, 3);
    dr.set(fbc_taint, 3);

    char fontname[256];
    fontname[0] = '\0';

    fs_inode consfont;
    if (fs_namei("/etc/consfont", &consfont) >= 0) {
	ssize_t cc = fs_pread(consfont, &fontname[0], sizeof(fontname), 0);
	if (cc >= 0) {
	    fontname[cc] = '\0';

	    char *nl = strchr(fontname, '\n');
	    if (nl)
		*nl = '\0';
	}
    }

    if (!fontname[0])
	sprintf(&fontname[0], "Monospace-20");

    const char *argv[] = { "fbconsd", a0, a1, fontname };
    child_process cp = spawn(start_env->process_pool,
		   ino, basecons, basecons, basecons,
		   4, &argv[0],
		   sizeof(env)/sizeof(env[0]), &env[0],
		   0, &ds, 0, &dr, 0, SPAWN_NO_AUTOGRANT);

    int64_t ec;
    error_check(process_wait(&cp, &ec));

    for (int vt = 0; vt < maxvt; vt++) {
	char namebuf[KOBJ_NAME_LEN];
	sprintf(&namebuf[0], "consbuf%d", vt);
	int64_t fbseg = container_find(cp.container, kobj_segment, namebuf);
	if (fbseg < 0)
	    return 1;

	char pnbuf[64];
	sprintf(&pnbuf[0], "#%"PRIu64".%"PRIu64, cp.container, fbseg);
	int fd = open(pnbuf, O_RDWR);
	if (fd < 0)
	    return 1;

	const char *msg = "init: graphics console initialized.\n";
	write(fd, msg, strlen(msg));
	consp[vt] = fd;
    }

    return maxvt;
}

int
main(int ac, char **av)
try
{
    uint64_t c_root = start_arg0;
    uint64_t h_root = start_arg1;

    int64_t c_self = container_find(c_root, kobj_container, "init");
    if (c_self < 0) {
	cprintf("cannot find init container: %s\n", e2s(c_self));
	return -1;
    }

    cprintf("JOS: init (root container %"PRIu64")\n", c_root);

    init_env(c_root, c_self, h_root);

    int cons = opencons();
    if (cons != 0) {
	cprintf("cannot opencons: %s\n", e2s(cons));
	return -1;
    }

    int r = fd_make_public(cons, 0);
    if (r < 0) {
	cprintf("cannot export cons: %s\n", e2s(r));
	return -1;
    }

    struct Fd *fd;
    r = fd_lookup(cons, &fd, 0, 0);
    if (r < 0) {
	cprintf("cannot lookup cons: %s\n", e2s(r));
	return -1;
    }

    fd->fd_immutable = 1;

    assert(1 == dup2(0, 1));
    assert(2 == dup2(0, 2));

    int64_t h_root_t = handle_alloc();
    if (h_root_t < 0) {
	cprintf("cannot allocate root taint handle: %s\n", e2s(h_root_t));
	return -1;
    }
    start_env->user_taint = h_root_t;

    start_arg0 = (uintptr_t) start_env;
    setup_env(0, start_arg0, 0);

    init_fs(cons);
    init_procs(cons);

    /* shell gets another console that's mutable */
    int newcons = opencons();
    if (newcons >= 0)
	cons = newcons;

    int cons_fds[6];
    int ncons = init_fbcons(cons, &cons_fds[0], 6);
    for (int i = 1; i < ncons; i++) {
	cobj_ref tid;
	thread_create(start_env->proc_container, &run_shell_entry,
		      (void *) (uintptr_t) cons_fds[i], &tid, "runshell");
    }

    run_shell(cons_fds[0]);
} catch (std::exception &e) {
    cprintf("init: %s\n", e.what());
    return -1;
}
