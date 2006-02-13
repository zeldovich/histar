#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>

static void
spawn_fs(int fd, const char *pn, int drop_root_handle)
{
    struct ulabel *l = label_get_current();
    if (l == 0)
	panic("cannot get current label\n");

    if (drop_root_handle)
	label_max_default(l);

    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	panic("cannot fs_lookup %s: %s\n", pn, e2s(r));

    const char *argv[] = { pn };
    r = spawn_fd(start_env->root_container, ino, fd, fd, fd, 1, &argv[0], l);
    if (r < 0)
	panic("cannot spawn %s: %s\n", pn, e2s(r));

    printf("init: spawned %s with label %s\n", pn, label_to_string(l));
    label_free(l);
}

int
main(int ac, char **av)
{
    uint64_t c_self = start_arg0;
    uint64_t c_root = start_arg1;

    assert(0 == opencons(c_self));
    assert(1 == dup(0, 1));
    assert(2 == dup(0, 2));

    start_env = 0;
    struct cobj_ref sa;
    assert(0 == segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env,
			      0, "init env"));

    start_env->container = c_self;
    start_env->root_container = c_root;

    int r = fs_get_root(c_root, &start_env->fs_root);
    if (r < 0)
	panic("fs_get_root: %s", e2s(r));

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

    start_env->fs_cwd = start_env->fs_root;
    printf("JOS: init (root container %ld)\n", c_root);

    // Now that we're set up a reasonable environment,
    // create a shared console fd.
    struct ulabel *label = label_get_current();
    assert(label);

    // This changes label to { h_root:0 1 }
    label_change_star(label, 0);
    segment_set_default_label(label);

    int cons = opencons(start_env->root_container);
    assert(cons >= 0);
    segment_set_default_label(0);

    // netd_mom should be the only process that needs our root handle at *,
    // in order to create an appropriately-labeled netdev object.
    spawn_fs(cons, "/bin/netd_mom", 0);

    //spawn_fs(cons, "/bin/shell", 0);
    spawn_fs(cons, "/bin/shell", 1);

    //spawn_fs(cons, "/bin/telnetd", 1);
    //spawn_fs(cons, "/bin/freelist_test", 1);
    spawn_fs(cons, "/bin/httpd", 1);

    for (;;)
	sys_thread_sleep(1000);
}
