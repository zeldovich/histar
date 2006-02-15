#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>

int64_t
spawn(uint64_t container, struct fs_inode elf_ino,
      int fd0, int fd1, int fd2, int ac, const char **av,
      struct ulabel *obj_l, struct ulabel *thread_l)
{
    int r;
    start_env_t *spawn_env = 0;
    int64_t c_spawn = -1;
    struct ulabel *obj_label = 0;
    struct ulabel *thread_label = 0;
    int64_t process_handle = -1;

    struct cobj_ref elf;
    r = fs_get_obj(elf_ino, &elf);
    if (r < 0) {
	printf("spawn: cannot convert inode to segment: %s\n", e2s(r));
	goto err;
    }

    obj_label = obj_l ? label_dup(obj_l) : label_get_current();
    if (obj_label == 0) {
	printf("spawn: cannot allocate object label\n");
	r = -E_NO_MEM;
	goto err;
    }

    thread_label = thread_l ? label_dup(thread_l) : label_get_current();
    if (thread_label == 0) {
	printf("spawn: cannot allocate thread label\n");
	r = -E_NO_MEM;
	goto err;
    }

    process_handle = sys_handle_create();
    if (process_handle < 0) {
	printf("spawn: cannot allocate handle: %s\n", e2s(process_handle));
	r = process_handle;
	goto err;
    }

    r = label_set_level(obj_label, process_handle, 0, 1);
    if (r < 0) {
	printf("spawn: cannot grant process handle: %s\n", e2s(r));
	goto err;
    }

    r = label_set_level(thread_label, process_handle, LB_LEVEL_STAR, 1);
    if (r < 0) {
	printf("spawn: cannot grant process handle: %s\n", e2s(r));
	goto err;
    }

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(elf, &name[0]);
    if (r < 0)
	goto err;

    c_spawn = sys_container_alloc(container, obj_label, &name[0]);
    struct cobj_ref c_spawn_ref = COBJ(container, c_spawn);
    if (c_spawn < 0) {
	printf("cannot allocate container for new thread: %s\n",
		e2s(c_spawn));
	r = c_spawn;
	goto err;
    }

    struct thread_entry e;
    r = elf_load(c_spawn, elf, &e, obj_label);
    if (r < 0) {
	printf("cannot load ELF: %s\n", e2s(r));
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
    r = segment_alloc(c_spawn, PGSIZE, &c_spawn_env, (void**) &spawn_env, obj_label, "env");
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
	printf("cannot create thread: %s\n", e2s(thread));
	r = thread;
	goto err;
    }
    struct cobj_ref tobj = COBJ(c_spawn, thread);

    e.te_arg = (uint64_t) spawn_env_va;
    r = sys_thread_start(tobj, &e, thread_label);
    if (r < 0) {
	printf("cannot start thread: %s\n", e2s(r));
	goto err;
    }

    goto out;

err:
    if (c_spawn >= 0)
	sys_obj_unref(c_spawn_ref);
    c_spawn = r;

out:
    if (obj_label)
	label_free(obj_label);
    if (thread_label)
	label_free(thread_label);
    if (spawn_env)
	segment_unmap(spawn_env);

    if (process_handle >= 0) {
	struct ulabel *label_self = label_get_current();
	if (label_self == 0) {
	    printf("spawn: cannot allocate self label for cleanup\n");
	} else {
	    assert(0 == label_set_level(label_self, process_handle,
					label_self->ul_default, 1));
	    assert(0 == label_set_current(label_self));
	    label_free(label_self);
	}
    }

    return c_spawn;
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
