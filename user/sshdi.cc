extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>

#include <stdlib.h>
#include <fcntl.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

int
main (int ac, char **av)
{
    try {
	if (ac < 2) {
	    cprintf("usage: %s grant-category\n", av[0]);
	    exit(-1);
	}
	
	int r;
	int64_t exit_code;
	struct fs_inode ino;
	
	start_env->fs_cwd = start_env->fs_root;
	
	r = fs_namei("/bin/tar", &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup /bin/tar");
	
	const char *argv0[] = { "/bin/tar", "xvf", "/bin/ssh.tar" };
	child_process cp = spawn(start_env->shared_container, ino,
				 0, 0, 0,
				 3, &argv0[0],
				 0, 0,
				 0, 0, 0, 0, 0);
	error_check(process_wait(&cp, &exit_code));
	error_check(exit_code);
	
	r = fs_namei("/bin/sshd", &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup /bin/sshd");
	
	const char *argv1[] = { "/bin/sshd", "-f", "/etc/sshd_config", "-r" };
	cp = spawn(start_env->shared_container, ino,
		   0, 0, 0,
		   4, &argv1[0],
		   0, 0,
		   0, 0, 0, 0, 0);
	error_check(process_wait(&cp, &exit_code));
	error_check(exit_code);
	
	return 0;
    } catch (std::exception &e) {
	cprintf("sshdi: main: %s\n", e.what());
	return -1;
    }
}

    
