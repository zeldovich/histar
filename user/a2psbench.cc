extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/string.h>

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
}


#include <inc/spawn.hh>
#include <inc/error.hh>
#include <inc/errno.hh>

const char *the_text = "hello a2ps!";

int
main(int ac, char **av)
{
    // Create test.txt in /fs to actually bench generating a ps file
    const char *av0[] = { "/bin/a2ps", "--output=/dev/null", "test.txt" };

    struct fs_inode ino;
    error_check(fs_namei("/bin/a2ps", &ino));

    int fd = 0;
    errno_check(fd = open("/dev/null", O_RDWR));

    uint64_t start = sys_clock_msec();
    uint64_t end = start;
    uint64_t time;
    if (ac >= 2) {
	error_check(strtou64(av[1], 0, 10, &time));
	end += time;
    }

    struct fs_inode root_ino;
    error_check(fs_namei("/fs", &root_ino));
    
    uint64_t i = 0;
    uint64_t stop;
    
    spawn_descriptor sd;
    sd.ct_ = start_env->proc_container;
    sd.elf_ino_ = ino;
    sd.fd0_ = fd;
    sd.fd1_ = fd;
    sd.fd2_ = fd;
    sd.ac_ = 3;
    sd.av_ = av0;
    sd.fs_root_ = root_ino;
    sd.fs_cwd_ = root_ino;
    
    for (; end > (stop = (uint64_t)sys_clock_msec()); i++) {	
	child_process cp = spawn(&sd);
	int64_t exit_code;
	error_check(process_wait(&cp, &exit_code));
	error_check(exit_code);    
	error_check(sys_obj_unref(COBJ(start_env->proc_container,
				       cp.container)));
    }

    printf("completed %ld in %ld msec\n", i, stop - start);
}



