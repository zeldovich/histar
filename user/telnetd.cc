extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
}

#include <inc/spawn.hh>

enum {
    T_IAC = 255,
    T_WILL = 251,
};

enum {
    T_ECHO = 1,
    T_SGA = 3,
};

static int enable_telnetopts = 0;

static void
telnet_server(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(23);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("telnetd: server on port 23\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	if (enable_telnetopts) {
	    char telnetopts[] = { T_IAC, T_WILL, T_ECHO,
				  T_IAC, T_WILL, T_SGA };
	    write(ss, &telnetopts[0], sizeof(telnetopts));
	}

	fd_set_isatty(ss, 1);

	struct fs_inode sh;
	const char *prog = "/bin/jshell";
	r = fs_namei(prog, &sh);
	if (r < 0) {
	    printf("cannot find %s: %s\n", prog, e2s(r));
	    close(ss);
	    continue;
	}

	const char *argv[1];
	argv[0] = "shell";

	spawn(start_env->shared_container, sh,
	      ss, ss, ss,
	      1, &argv[0], 
	      0, 0, 0, 0,
	      SPAWN_MOVE_FD);
	close(ss);
    }
}

int
main(int ac, char **av)
{
    telnet_server();
}
