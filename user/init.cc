extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/fd.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

static void
spawn_fs(int fd, const char *pn, const char *arg, label *tl)
{
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	throw error(r, "cannot fs_lookup %s", pn);

    const char *argv[] = { pn, arg };
    int64_t ct = spawn(start_env->root_container, ino,
		       fd, fd, fd,
		       arg ? 2 : 1, &argv[0],
		       tl->to_ulabel(), tl->to_ulabel(),
		       0);
    if (ct < 0)
	throw error(ct, "cannot spawn %s", pn);

    printf("init: spawned %s with label %s\n", pn, tl->to_string());
}

static void
init_env(uint64_t c_root, uint64_t c_self)
{
    start_env = 0;
    struct cobj_ref sa;
    error_check(segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env,
			      0, "init env"));

    start_env->container = c_self;
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

static int
init_console(uint64_t h_root)
{
    // Now that we're set up a reasonable environment,
    // create a shared console fd.
    struct ulabel *label = label_alloc();
    assert(label);

    label->ul_default = 1;
    error_check(label_set_level(label, h_root, 0, 1));
    segment_set_default_label(label);

    int cons = opencons(start_env->root_container);
    assert(cons >= 0);
    segment_set_default_label(0);

    return cons;
}

static void
init_procs(int cons, uint64_t h_root)
{
    label star(LB_LEVEL_STAR);
    label without_hroot(1);
    label with_hroot(1);
    with_hroot.set(h_root, LB_LEVEL_STAR);

    int64_t h_adm = sys_handle_create();
    error_check(h_adm);

    char h_adm_buf[32];
    snprintf(&h_adm_buf[0], sizeof(h_adm_buf), "%lu", h_adm);

    // netd_mom should be the only process that needs { h_root:* },
    // in order to create an appropriately-labeled netdev object.
    spawn_fs(cons, "/bin/netd_mom", 0, &with_hroot);

    // admin server gets { * }
    spawn_fs(cons, "/bin/admind", &h_adm_buf[0], &star);

    //spawn_fs(cons, "/bin/shell", 0, &with_hroot);
    spawn_fs(cons, "/bin/shell", 0, &without_hroot);

    //spawn_fs(cons, "/bin/telnetd", 0, &without_hroot);
    //spawn_fs(cons, "/bin/httpd", 0, &without_hroot);
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

    assert(0 == opencons(c_self));
    assert(1 == dup(0, 1));
    assert(2 == dup(0, 2));

    printf("JOS: init (root container %ld)\n", c_root);

    init_env(c_root, c_self);
    int cons = init_console(h_root);
    init_procs(cons, h_root);

    for (;;)
	thread_sleep(100000);
} catch (std::exception &e) {
    printf("init: %s\n", e.what());
}
