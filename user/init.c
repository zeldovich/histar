#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>

static void
spawn_fs(const char *pn)
{
    struct cobj_ref fsobj;
    int r = fs_lookup(start_env->fs_root, pn, &fsobj);
    if (r < 0)
	panic("cannot fs_lookup %s: %s\n", pn, e2s(r));

    r = spawn(start_env->root_container, fsobj);
    if (r < 0)
	panic("cannot spawn %s: %s\n", pn, e2s(r));

    printf("init: spawned %s\n", pn);
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
    assert(0 == segment_alloc(c_self, PGSIZE, &sa, (void**) &start_env));

    start_env->container = c_self;
    start_env->root_container = c_root;

    int r = fs_get_root(c_root, &start_env->fs_root);
    if (r < 0)
	panic("fs_get_root: %s", e2s(r));

    printf("JOS: init (root container %ld)\n", c_root);

    spawn_fs("shell");
    spawn_fs("netd");
    spawn_fs("telnetd");

    return 0;
}
