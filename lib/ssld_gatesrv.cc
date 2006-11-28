extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/ssld.h>
#include <inc/error.h>

#include <fcntl.h>
}

#include <inc/cpplabel.hh>
#include <inc/spawn.hh>

static const char nullio = 0;

int
ssld_server_init(uint64_t taint, const char *server_pem, const char *password, 
		 const char *calist_pem, const char *dh_pem)
{
    struct fs_inode ssl_ino;
    int r = fs_namei("/bin/ssld", &ssl_ino);
    if (r < 0)
	return r;

    label co(0);
    label cs(LB_LEVEL_STAR);
    
    // XXX will need an appropriate netd...
    if (taint) {
	co.set(taint, 2);
	cs.set(taint, 2);
    }
    
    int io;
    if (nullio) {
	int nullfd = open("/dev/null", O_RDONLY);
	if (nullfd < 0) {
	    cprintf("ssld_server_init: cannot open /dev/null: %s\n", 
		    e2s(nullfd));
	    return nullfd;
	}
	io = nullfd;
    } else {
	io = 0;
    }
    
    const char *argv[] = { "ssld", server_pem, password, calist_pem, dh_pem };
    struct child_process cp = spawn(start_env->shared_container, ssl_ino,
				    io, io, io,
				    5, &argv[0],
				    0, 0,
				    &cs, 0, 0, 0, &co);
    int64_t exit_code = 0;
    process_wait(&cp, &exit_code);
    if (exit_code) {
	cprintf("ssld_server_init: ssld exit_code %ld\n", exit_code);
	return -E_UNSPEC;
    }
    return 0;
}
