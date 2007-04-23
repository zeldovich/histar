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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>

static int init_debug = 0;
static const char *env[] = { "USER=root", "HOME=/" };
static uint64_t time_grant;

static struct child_process
spawn_fs(int fd, const char *pn, const char *arg, label *ds, label *dr)
{
    struct child_process cp;
    try {
	struct fs_inode ino;
	int r = fs_namei(pn, &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup %s", pn);

	const char *argv[] = { pn, arg };
	cp = spawn(start_env->root_container, ino,
	           fd, fd, fd,
	           arg ? 2 : 1, &argv[0],
		   sizeof(env)/sizeof(env[0]), &env[0],
	           0, ds, 0, dr, 0, SPAWN_NO_AUTOGRANT);

	if (init_debug)
	    printf("init: spawned %s, ds = %s\n", pn, ds->to_string());
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
    tods->unix_nsec_offset = NSEC_PER_SECOND * 1000000000UL;

    // set the filesystem root to be the same as the container root
    fs_get_root(c_root, &start_env->fs_root);

    // start out in the root directory
    start_env->fs_cwd = start_env->fs_root;
}

static void
init_fs(void)
{
    uint64_t c_root = start_env->root_container;

    // mount binaries on /bin
    int64_t fs_bin_id = container_find(c_root, kobj_container, "embed_bins");
    if (fs_bin_id < 0)
	throw error(fs_bin_id, "cannot find /bin");

    struct fs_inode bin_dir;
    fs_get_root(fs_bin_id, &bin_dir);
    error_check(fs_mount(start_env->fs_mtab_seg, start_env->fs_root, "bin", bin_dir));

    error_check(sys_container_alloc(start_env->root_container, 0, "uauth",
				    0, CT_QUOTA_INF));

    // create a hard link "sh" to "ksh" using the mount table.
    struct fs_inode fs_ksh;
    error_check(fs_namei("/bin/ksh", &fs_ksh));
    error_check(fs_mount(start_env->fs_mtab_seg, bin_dir, "sh", fs_ksh));

    // create a /dev directory
    struct fs_inode dev;
    error_check(fs_mkdir(start_env->fs_root, "dev", &dev, 0));

    struct fs_inode dummy_ino;
    error_check(fs_mknod(dev, "null", 'n', 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "zero", 'z', 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "tty", 'c', 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "random", 'r', 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "urandom", 'r', 0, &dummy_ino, 0));
    error_check(fs_mknod(dev, "ptmx", 'x', 0, &dummy_ino, 0));
    error_check(fs_mkdir(dev, "pts", &dummy_ino, 0));

    // create a /home directory
    struct fs_inode fs_home;
    error_check(fs_mkdir(start_env->fs_root, "home", &fs_home, 0));

    // create a /fs directory
    struct fs_inode fs_root;
    error_check(fs_mkdir(start_env->fs_root, "fs", &fs_root, 0));
    error_check(fs_mkdir(fs_root, "tmp", &dummy_ino, 0));
    error_check(fs_mount(start_env->fs_mtab_seg, fs_root, "bin", bin_dir));
    error_check(fs_mount(start_env->fs_mtab_seg, fs_root, "dev", dev));
    error_check(fs_mount(start_env->fs_mtab_seg, fs_root, "home", fs_home));

    // create a /share directory
    error_check(fs_mkdir(start_env->fs_root, "share", &dummy_ino, 0));

    // create a scratch container
    label ltmp(1);
    error_check(fs_mkdir(start_env->fs_root, "tmp", &dummy_ino, ltmp.to_ulabel()));

    struct fs_inode etc;
    error_check(fs_mkdir(start_env->fs_root, "etc", &etc, 0));

    struct fs_inode hosts, resolv, passwd, group;
    error_check(fs_create(etc, "hosts", &hosts, 0));
    error_check(fs_create(etc, "resolv.conf", &resolv, 0));
    error_check(fs_create(etc, "passwd", &passwd, 0));
    error_check(fs_create(etc, "group", &group, 0));

    const char *resolv_conf = "nameserver 171.66.3.11\nnameserver 171.66.3.10\n";
    error_check(fs_pwrite(resolv, resolv_conf, strlen(resolv_conf), 0));

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
}

static void
init_auth(int cons, const char *shroot)
{
    struct child_process cp;
    int64_t ec;

    cp = spawn_fs(cons, "/bin/auth_log", 0, 0, 0);
    error_check(process_wait(&cp, &ec));

    cp = spawn_fs(cons, "/bin/auth_dir", shroot, 0, 0);
    error_check(process_wait(&cp, &ec));

    // spawn user-auth agent for root
    fs_inode uauth_dir;
    error_check(fs_namei("/uauth", &uauth_dir));

    int64_t root_user_ct = sys_container_alloc(uauth_dir.obj.object, 0,
					       "root", 0, CT_QUOTA_INF);
    error_check(root_user_ct);

    fs_inode user_authd;
    error_check(fs_namei("/bin/auth_user", &user_authd));

    char root_grant[32], root_taint[32];
    sprintf(&root_grant[0], "%"PRIu64, start_env->user_grant);
    sprintf(&root_taint[0], "%"PRIu64, start_env->user_taint);
    const char *argv[] = { "auth_user", root_grant, root_grant, root_taint };

    cp = spawn(root_user_ct, user_authd, cons, cons, cons,
               sizeof(argv)/sizeof(argv[0]), argv, 0, 0,
               0, 0, 0, 0, 0);
    error_check(process_wait(&cp, &ec));

    // register this user-agent with the auth directory
    int64_t uauth_gate;
    error_check(uauth_gate =
	container_find(cp.container, kobj_gate, "user login gate"));

    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "auth_dir"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "authdir"));

    gate_call_data gcd;
    auth_dir_req   *req   = (auth_dir_req *)   &gcd.param_buf[0];
    auth_dir_reply *reply = (auth_dir_reply *) &gcd.param_buf[0];
    req->op = auth_dir_add;
    strcpy(&req->user[0], "root");
    req->user_gate = COBJ(cp.container, uauth_gate);

    label verify(3);
    verify.set(start_env->user_grant, 0);
    gate_call(COBJ(dir_ct, dir_gt), 0, 0, 0).call(&gcd, &verify);
    error_check(reply->err);
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
    spawn_fs(cons, "/bin/jntpd", "ntp.stanford.edu", &time_ds, &time_dr);

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
	    
	    if (priv[0] == 'r')
		spawn_fs(cons, fn, &root_grant_buf[0], &root_ds, &root_dr);
	    else 
		spawn_fs(cons, fn, &root_grant_buf[0], 0, 0);
	}
    }
    spawn_fs(cons, "/bin/ksh", "/bin/init.sh", 0, 0);
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
        struct child_process shell_proc = spawn_fs(cons, "/bin/ksh", 0, &ds, &dr);
        int64_t exit_code = 0;
        if ((r = process_wait(&shell_proc, &exit_code)) < 0)
            cprintf("run_shell: process_wait error: %s\n", e2s(r));
        else
            cprintf("run_shell: shell exit value %"PRId64"\n", exit_code);
	sys_obj_unref(COBJ(start_env->root_container, shell_proc.container));
    }
}

int
main(int ac, char **av)
try
{
    uint64_t c_root = start_arg0;
    uint64_t h_root = start_arg1;

    int64_t c_self = container_find(c_root, kobj_container, "init");
    if (c_self < 0)
	throw error(c_self, "cannot find init container");

    cprintf("JOS: init (root container %"PRIu64")\n", c_root);

    init_env(c_root, c_self, h_root);

    int cons = opencons();
    if (cons != 0)
	throw error(cons, "cannot opencons: %d", cons);

    assert(1 == dup2(0, 1));
    assert(2 == dup2(0, 2));

    int64_t h_root_t = handle_alloc();
    if (h_root_t < 0)
	throw error(h_root_t, "cannot allocate root taint handle");
    start_env->user_taint = h_root_t;

    setup_env(0, (uintptr_t) start_env, 0);

    init_fs();
    init_procs(cons);
    run_shell(cons);	// does not return
} catch (std::exception &e) {
    cprintf("init: %s\n", e.what());
}
