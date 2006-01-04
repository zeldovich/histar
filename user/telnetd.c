#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>

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

    printf("netd: server on port 23\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	// just to test other things while i'm debugging the spawn code below
	write(ss, "Hello world.\n", 13);
	close(ss);
	continue;

	struct cobj_ref sh;
	r = fs_lookup(start_env->fs_root, "shell", &sh);
	if (r < 0) {
	    printf("cannot find shell: %s\n", e2s(r));
	    close(ss);
	    continue;
	}

	int64_t sp = spawn_fd(start_env->root_container, sh, ss, ss, ss);
	if (sp < 0) {
	    printf("cannot spawn shell: %s\n", e2s(sp));
	    close(ss);
	    continue;
	}

	close(ss);
    }
}

int
main(int ac, char **av)
{
    int r = netd_client_init();
    if (r < 0)
	panic("initializing netd client: %s", e2s(r));

    telnet_server();
}
