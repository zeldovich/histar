extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <stdio.h>
}

#include <inc/spawn.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

int
main(int ac, char **av)
{
    if (ac < 3) {
	printf("usage: %s new-root command [args]\n", av[0]);
	return -1;
    }
    
    struct fs_inode elf_ino, root_ino;
    int r = fs_namei(av[1], &root_ino);
    if (r < 0) {
	printf("chroot: unable to fs_namei %s: %s\n", av[1], e2s(r));
	return -1;
    }
    r = fs_namei(av[2], &elf_ino);
    if (r < 0) {
	printf("chroot: unable to fs_namei %s: %s\n", av[2], e2s(r));
	return -1;
    }

    spawn_descriptor sd;
    sd.ct_ = start_env->shared_container;
    sd.elf_ino_ = elf_ino;

    sd.fd0_ = 0;
    sd.fd1_ = 1;
    sd.fd2_ = 2;
    
    sd.ac_ = ac - 2;
    sd.av_ = (const char **)av + 2;
    
    sd.fs_root_ = root_ino;
    sd.fs_cwd_ = root_ino;
    
    struct child_process cp = spawn(&sd);
    scope_guard<int, struct cobj_ref> 
	unref(sys_obj_unref, COBJ(start_env->shared_container, cp.container));
    int64_t exit_code;
    try {
	error_check(process_wait(&cp, &exit_code));
	printf("chroot: exit code %ld\n", exit_code);
    } catch (basic_exception &e) {
	printf("chroot: %s\n", e.what());
    }

    return 0;
}
