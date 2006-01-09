#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>

static void
http_client(void *arg)
{
    int s = (int64_t) arg;
    char buf[1024];

    int cc = read(s, &buf[0], sizeof(buf));
    if (cc < 0)
	printf("httpd: cannot read request: %s\n", e2s(cc));

    char *resp = "<h1>jos64 web server</h1><p>foo.\n";

    write(s, resp, strlen(resp));
    close(s);

    sys_obj_unref(COBJ(start_env->container, thread_id()));
    sys_thread_halt();

    // XXX this doesn't GC the thread stack!
}

static void
http_server(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(80);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("httpd: server on port 80\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	struct cobj_ref t;
	int r = thread_create(start_env->container, &http_client,
			      (void*) (int64_t) ss, &t);
	if (r < 0) {
	    printf("cannot spawn client thread: %s\n", e2s(r));
	    close(ss);
	}
    }
}

int
main(int ac, char **av)
{
    http_server();
}
