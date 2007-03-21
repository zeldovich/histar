extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/string.h>

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
}


#include <inc/spawn.hh>
#include <inc/error.hh>
#include <inc/errno.hh>

int
main(int ac, char **av)
{
    // Create test.ps in /fs to actually bench generating a pdf file
    const char *av0[] = { "/bin/gs", 
			  "-q",     
			  "-dNOPAUSE", 
			  "-dBATCH", 
			  "-sDEVICE=pdfwrite", 
			  "-sOutputFile=/dev/null", 
			  "-c",
			  ".setpdfwrite",
			  "-f",
			  "test.ps" };
    
    struct fs_inode ino;
    error_check(fs_namei("/bin/gs", &ino));

    int fd = 0;
    errno_check(fd = open("/dev/null", O_RDWR));

    uint64_t start = sys_clock_nsec();
    uint64_t end = start;
    if (ac >= 2) {
	uint64_t time;
	error_check(strtou64(av[1], 0, 10, &time));
	end += NSEC_PER_SECOND / 1000 * time;
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
    sd.ac_ = 10;
    sd.av_ = av0;
    sd.fs_root_ = root_ino;
    sd.fs_cwd_ = root_ino;
    
    for (; end > (stop = (uint64_t)sys_clock_nsec()); i++) {
	child_process cp = spawn(&sd);
	int64_t exit_code;
	error_check(process_wait(&cp, &exit_code));
	error_check(exit_code);    
	error_check(sys_obj_unref(COBJ(start_env->proc_container,
				       cp.container)));
    }

    printf("completed %ld in %ld nsec\n", i, stop - start);
}



