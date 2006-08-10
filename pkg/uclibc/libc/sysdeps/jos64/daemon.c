#include <unistd.h>
#include <fcntl.h>
#include <paths.h>

int
daemon(int nochdir, int noclose)
{
    int fd;
        
    if (setsid() == -1)
	return(-1);
        
    if (!nochdir)
	chdir("/");
    
    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	if (fd > 2)
	    close(fd);
    }
    return(0);
}
