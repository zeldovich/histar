#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>

int64_t
spawn_fd(uint64_t container, struct cobj_ref elf,
	 int fd0, int fd1, int fd2)
{
    int64_t c_spawn = sys_container_alloc(container);
    struct cobj_ref c_spawn_ref = COBJ(container, c_spawn);
    if (c_spawn < 0) {
	cprintf("cannot allocate container for new thread: %s\n",
		e2s(c_spawn));
	return c_spawn;
    }

    struct thread_entry e;
    int r = elf_load(c_spawn, elf, &e);
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

    start_env_t *spawn_env = 0;
    struct cobj_ref c_spawn_env;
    r = segment_alloc(c_spawn, PGSIZE, &c_spawn_env, (void**) &spawn_env);
    if (r < 0)
	goto err;

    void *spawn_env_va = 0;
    r = segment_map_as(e.te_as, c_spawn_env, SEGMAP_READ | SEGMAP_WRITE,
		       &spawn_env_va, 0);
    if (r < 0)
	goto err;

    memcpy(spawn_env, start_env, sizeof(*spawn_env));
    spawn_env->container = c_spawn;

    int64_t thread = sys_thread_create(c_spawn);
    if (thread < 0) {
	cprintf("cannot create thread: %s\n", e2s(thread));
	r = thread;
	goto err;
    }

    e.te_arg = (uint64_t) spawn_env_va;

    r = sys_thread_start(COBJ(c_spawn, thread), &e);
    if (r < 0) {
	cprintf("cannot start thread: %s\n", e2s(r));
	goto err;
    }

    return c_spawn;

err:
    sys_obj_unref(c_spawn_ref);
    return r;
}

int64_t
spawn(uint64_t container, struct cobj_ref elf)
{
    return spawn_fd(container, elf, 0, 1, 2);
}
