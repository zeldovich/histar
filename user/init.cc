extern "C" {
#include <inc/stdio.h>
#include <stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <unistd.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>

static int init_debug = 0;

static void
spawn_fs(int fd, const char *pn, const char *arg, label *ds)
{
    try {
	struct fs_inode ino;
	int r = fs_namei(pn, &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup %s", pn);

	const char *argv[] = { pn, arg };
	spawn(start_env->root_container, ino,
	      fd, fd, fd,
	      arg ? 2 : 1, &argv[0],
	      0, ds, 0, 0,
	      0);

	if (init_debug)
	    printf("init: spawned %s, ds = %s\n", pn, ds->to_string());
    } catch (std::exception &e) {
	cprintf("spawn_fs(%s): %s\n", pn, e.what());
    }
}

static void
init_env(uint64_t c_root, uint64_t c_self)
{
    start_env = 0;
    struct cobj_ref sa;
    error_check(segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env,
			      0, "init env"));

    start_env->proc_container = c_self;
    start_env->shared_container = c_self;
    start_env->root_container = c_root;

    // set the filesystem root to be the same as the container root
    int r = fs_get_root(c_root, &start_env->fs_root);
    if (r < 0)
	throw error(r, "fs_get_root");

    // mount binaries on /bin
    int64_t fs_bin_id = container_find(c_root, kobj_container, "fs root");
    if (fs_bin_id < 0)
	throw error(fs_bin_id, "cannot find /bin");

    struct fs_inode bin_dir;
    r = fs_get_root(fs_bin_id, &bin_dir);
    if (r < 0)
	throw error(r, "fs_get_root for /bin");

    r = fs_mount(start_env->fs_root, "bin", bin_dir);
    if (r < 0)
	throw error(r, "fs_mount /bin");

    // create a scratch container
    struct fs_inode scratch;
    r = fs_mkdir(start_env->fs_root, "x", &scratch);
    if (r < 0)
	throw error(r, "creating /x");

    // start out in the root directory
    start_env->fs_cwd = start_env->fs_root;
}

static void
init_procs(int cons, uint64_t h_root)
{
    label ds_star(LB_LEVEL_STAR);
    label ds_none(3);
    label ds_hroot(3);
    ds_hroot.set(h_root, LB_LEVEL_STAR);

    int64_t h_adm = sys_handle_create();
    error_check(h_adm);

    char h_adm_buf[32];
    snprintf(&h_adm_buf[0], sizeof(h_adm_buf), "%lu", h_adm);

    // netd_mom should be the only process that needs { h_root:* },
    // in order to create an appropriately-labeled netdev object.
    spawn_fs(cons, "/bin/netd_mom", 0, &ds_hroot);

    // admin server gets { * }
    spawn_fs(cons, "/bin/admind", &h_adm_buf[0], &ds_star);

    spawn_fs(cons, "/bin/authd", 0, &ds_none);

    //spawn_fs(cons, "/bin/shell", 0, &ds_hroot);
    spawn_fs(cons, "/bin/shell", 0, &ds_none);

    //spawn_fs(cons, "/bin/telnetd", 0, &ds_none);
    //spawn_fs(cons, "/bin/httpd", 0, &ds_none);
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

    init_env(c_root, c_self);

    int cons = opencons(start_env->shared_container);
    if (cons != 0)
	throw error(cons, "cannot opencons: %d", cons);

    assert(1 == dup2(0, 1));
    assert(2 == dup2(0, 2));

    init_procs(cons, h_root);

    for (;;)
	thread_sleep(100000);
} catch (std::exception &e) {
    cprintf("init: %s\n", e.what());
}
