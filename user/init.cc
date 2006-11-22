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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>

static int init_debug = 0;
static const char *env[] = { "USER=root", "HOME=/" };

static struct child_process
spawn_fs(int fd, const char *pn, const char *arg, label *ds)
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
	           0, ds, 0, 0, 0);

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

    label mtab_label(1);
    error_check(segment_alloc(c_self, sizeof(struct fs_mount_table),
			      &start_env->fs_mtab_seg, 0,
			      mtab_label.to_ulabel(), "mount table"));

    // set the filesystem root to be the same as the container root
    fs_get_root(c_root, &start_env->fs_root);

    // mount binaries on /bin
    int64_t fs_bin_id = container_find(c_root, kobj_container, "fs root");
    if (fs_bin_id < 0)
	throw error(fs_bin_id, "cannot find /bin");

    struct fs_inode bin_dir;
    fs_get_root(fs_bin_id, &bin_dir);
    error_check(fs_mount(start_env->fs_root, "bin", bin_dir));

    error_check(sys_container_alloc(start_env->root_container, 0, "uauth",
				    0, CT_QUOTA_INF));

    // create a /home directory
    struct fs_inode home;
    error_check(fs_mkdir(start_env->fs_root, "home", &home, 0));

    // create a scratch container
    label lx(1);

    struct fs_inode scratch;
    error_check(fs_mkdir(start_env->fs_root, "tmp", &scratch, lx.to_ulabel()));

    struct fs_inode etc;
    error_check(fs_mkdir(start_env->fs_root, "etc", &etc, 0));

    struct fs_inode hosts, resolv, passwd, group;
    error_check(fs_create(etc, "hosts", &hosts, 0));
    error_check(fs_create(etc, "resolv.conf", &resolv, 0));
    error_check(fs_create(etc, "passwd", &passwd, 0));
    error_check(fs_create(etc, "group", &group, 0));

    const char *resolv_conf = "nameserver 171.66.3.11\n";
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

    // start out in the root directory
    start_env->fs_cwd = start_env->fs_root;
}

static void
init_procs(int cons, uint64_t h_root, uint64_t h_root_t)
{
    label ds_none(3);
    label ds_hroot(3);
    ds_hroot.set(h_root, LB_LEVEL_STAR);
    ds_hroot.set(h_root_t, LB_LEVEL_STAR);

    char h_root_buf[32];
    snprintf(&h_root_buf[0], sizeof(h_root_buf), "%lu", h_root);

    spawn_fs(cons, "/bin/netd_mom", &h_root_buf[0], &ds_hroot);
    spawn_fs(cons, "/bin/auth_log", &h_root_buf[0], &ds_none);
    spawn_fs(cons, "/bin/auth_dir", &h_root_buf[0], &ds_none);

    spawn_fs(cons, "/bin/httpd", &h_root_buf[0], &ds_hroot);
    spawn_fs(cons, "/bin/httpd_worker", &h_root_buf[0], &ds_hroot);

    //spawn_fs(cons, "/bin/admind", &h_root_buf[0], &ds_hroot);
    //spawn_fs(cons, "/bin/rbac_init", &h_root_buf[0], &ds_hroot);
    //spawn_fs(cons, "/bin/login", 0, &ds_none);

    FILE *init_init = fopen("/bin/init.init", "r");
    if (init_init) {
	char buf[256];
	while (fgets(buf, sizeof(buf), init_init)) {
	    char *fn = &buf[0];
	    char *priv = fn;
	    strsep(&priv, ":");
	    if (!priv) {
		cprintf("init_procs: bad entry? %s\n", buf);
		continue;
	    }
	    
	    if (priv[0] == 'r')
		spawn_fs(cons, fn, &h_root_buf[0], &ds_hroot);
	    else 
		spawn_fs(cons, fn, &h_root_buf[0], &ds_none);
	}
    }
}

static void
run_shell(int cons, uint64_t h_root, uint64_t h_root_t)
{
    int r;
    label ds_hroot(3);
    ds_hroot.set(h_root, LB_LEVEL_STAR);
    ds_hroot.set(h_root_t, LB_LEVEL_STAR);

    for (;;) {
        struct child_process shell_proc = spawn_fs(cons, "/bin/ksh", 0, &ds_hroot);    
        int64_t exit_code = 0;
        if ((r = process_wait(&shell_proc, &exit_code)) < 0)
            cprintf("run_shell: process_wait error: %s\n", e2s(r));
        else
            cprintf("run_shell: shell exit value %ld\n", exit_code);
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

    cprintf("JOS: init (root container %ld)\n", c_root);

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

    init_procs(cons, h_root, h_root_t);
    // does not return
    run_shell(cons, h_root, h_root_t);
} catch (std::exception &e) {
    cprintf("init: %s\n", e.what());
}
