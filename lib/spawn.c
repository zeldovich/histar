#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>

int64_t
spawn_fd(uint64_t container, struct fs_inode elf_ino,
	 int fd0, int fd1, int fd2, int ac, const char **av,
	 struct ulabel *l)
{
    int r;
    start_env_t *spawn_env = 0;
    int64_t c_spawn = -1;

    struct cobj_ref elf;
    r = fs_get_obj(elf_ino, &elf);
    if (r < 0) {
	cprintf("spawn_fs: cannot convert inode to segment: %s\n", e2s(r));
	return r;
    }

    struct ulabel *label = l ? label_dup(l) : label_get_current();
    if (label == 0) {
	cprintf("spawn_fd: cannot allocate label\n");
	return -E_NO_MEM;
    }

    int64_t process_handle = sys_handle_create();
    if (process_handle < 0) {
	cprintf("spawn_fd: cannot allocate handle: %s\n", e2s(process_handle));
	return process_handle;
    }

    r = label_set_level(label, process_handle, 0, 1);
    if (r < 0) {
	cprintf("spawn_fd: cannot grant process handle: %s\n", e2s(r));
	goto err;
    }

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(elf, &name[0]);
    if (r < 0)
	goto err;

    c_spawn = sys_container_alloc(container, label, &name[0]);
    struct cobj_ref c_spawn_ref = COBJ(container, c_spawn);
    if (c_spawn < 0) {
	cprintf("cannot allocate container for new thread: %s\n",
		e2s(c_spawn));
	r = c_spawn;
	goto err;
    }

    struct thread_entry e;
    r = elf_load(c_spawn, elf, &e, label);
    if (r < 0) {
	cprintf("cannot load ELF: %s\n", e2s(r));
	goto err;
    }

    r = dup_as(fd0, 0, e.te_as);
    if (r < 0)
	goto err;

    r = dup_as(fd1, 1, e.te_as);
    if (r < 0)
	goto err;

    r = dup_as(fd2, 2, e.te_as);
    if (r < 0)
	goto err;

    struct cobj_ref c_spawn_env;
    r = segment_alloc(c_spawn, PGSIZE, &c_spawn_env, (void**) &spawn_env, label, "env");
    if (r < 0)
	goto err;

    void *spawn_env_va = 0;
    r = segment_map_as(e.te_as, c_spawn_env, SEGMAP_READ | SEGMAP_WRITE,
		       &spawn_env_va, 0);
    if (r < 0)
	goto err;

    memcpy(spawn_env, start_env, sizeof(*spawn_env));
    spawn_env->parent_container = container;
    spawn_env->container = c_spawn;

    char *p = &spawn_env->args[0];
    for (int i = 0; i < ac; i++) {
	size_t len = strlen(av[i]);
	memcpy(p, av[i], len);
	p += len + 1;
    }

    int64_t thread = sys_thread_create(c_spawn, &name[0]);
    if (thread < 0) {
	cprintf("cannot create thread: %s\n", e2s(thread));
	r = thread;
	goto err;
    }
    struct cobj_ref tobj = COBJ(c_spawn, thread);

    r = label_set_level(label, process_handle, LB_LEVEL_STAR, 1);
    if (r < 0) {
	cprintf("spawn_fd: cannot grant process handle *: %s\n", e2s(r));
	goto err;
    }

    e.te_arg = (uint64_t) spawn_env_va;
    r = sys_thread_start(tobj, &e, label);
    if (r < 0) {
	cprintf("cannot start thread: %s\n", e2s(r));
	goto err;
    }

    goto out;

err:
    if (c_spawn >= 0)
	sys_obj_unref(c_spawn_ref);
    c_spawn = r;

out:
    if (label)
	label_free(label);
    if (spawn_env)
	segment_unmap(spawn_env);

    struct ulabel *label_self = label_get_current();
    if (label_self == 0) {
	cprintf("spawn_fd: cannot allocate self label for cleanup\n");
    } else {
	assert(0 == label_set_level(label_self, process_handle,
				    label_self->ul_default, 1));
	assert(0 == label_set_current(label_self));
	label_free(label_self);
    }

    return c_spawn;
}

int64_t
spawn(uint64_t container, struct fs_inode elf, int ac, const char **av)
{
    return spawn_fd(container, elf, 0, 1, 2, ac, av, 0);
}

int
spawn_wait(uint64_t childct)
{
    for (;;) {
	int64_t obj = container_find(childct, kobj_segment, "dead flag");
	if (obj < 0) {
	    if (obj != -E_NOT_FOUND)
		return obj;
	    sys_thread_sleep(100);
	} else {
	    return 0;
	}
    }
}
