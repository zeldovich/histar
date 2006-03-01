#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/fd.h>

static void
spawn_fs(int fd, const char *pn, struct ulabel *thread_label)
{
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	panic("cannot fs_lookup %s: %s\n", pn, e2s(r));

    const char *argv[] = { pn };
    int64_t ct = spawn(start_env->root_container, ino,
		       fd, fd, fd, 1, &argv[0],
		       thread_label, thread_label,
		       0);
    if (ct < 0)
	panic("cannot spawn %s: %s\n", pn, e2s(ct));

    printf("init: spawned %s with label %s\n",
	   pn, label_to_string(thread_label));
}

static void
init_env(uint64_t c_root, uint64_t c_self)
{
    start_env = 0;
    struct cobj_ref sa;
    assert(0 == segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env,
			      0, "init env"));

    start_env->container = c_self;
    start_env->root_container = c_root;

    // set the filesystem root to be the same as the container root
    int r = fs_get_root(c_root, &start_env->fs_root);
    if (r < 0)
	panic("fs_get_root: %s", e2s(r));

    // mount binaries on /bin
    int64_t fs_bin_id = container_find(c_root, kobj_container, "fs root");
    if (fs_bin_id < 0)
	panic("cannot find fs /bin directory: %s", e2s(fs_bin_id));

    struct fs_inode bin_dir;
    r = fs_get_root(fs_bin_id, &bin_dir);
    if (r < 0)
	panic("fs_get_root for /bin: %s", e2s(r));

    r = fs_mount(start_env->fs_root, "bin", bin_dir);
    if (r < 0)
	panic("fs_mount for /bin: %s", e2s(r));

    // create a scratch container
    struct fs_inode scratch;
    r = fs_mkdir(start_env->fs_root, "x", &scratch);
    if (r < 0)
	panic("creating /x: %s", e2s(r));

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
    assert(0 == label_set_level(label, h_root, 0, 1));
    segment_set_default_label(label);

    int cons = opencons(start_env->root_container);
    assert(cons >= 0);
    segment_set_default_label(0);

    return cons;
}

static void
init_procs(int cons, uint64_t h_root)
{
    struct ulabel *with_hroot = label_alloc();
    struct ulabel *without_hroot = label_alloc();
    assert(with_hroot && without_hroot);

    with_hroot->ul_default = 1;
    without_hroot->ul_default = 1;
    assert(0 == label_set_level(with_hroot, h_root, LB_LEVEL_STAR, 1));

    // netd_mom should be the only process that needs our root handle at *,
    // in order to create an appropriately-labeled netdev object.
    spawn_fs(cons, "/bin/netd_mom", with_hroot);

    //spawn_fs(cons, "/bin/shell", with_hroot);
    spawn_fs(cons, "/bin/shell", without_hroot);

    //spawn_fs(cons, "/bin/telnetd", without_hroot);
    //spawn_fs(cons, "/bin/httpd", without_hroot);
}

int
main(int ac, char **av)
{
    uint64_t c_root = start_arg0;
    uint64_t h_root = start_arg1;

    int64_t c_self = container_find(c_root, kobj_container, "init");
    if (c_self < 0)
	panic("cannot find init container: %s", e2s(c_self));

    assert(0 == opencons(c_self));
    assert(1 == dup(0, 1));
    assert(2 == dup(0, 2));

    printf("JOS: init (root container %ld)\n", c_root);

    init_env(c_root, c_self);
    int cons = init_console(h_root);
    init_procs(cons, h_root);

    for (;;)
	thread_sleep(100000);
}
