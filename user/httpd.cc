extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>

#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/spawn.hh>

static void
http_client(void *arg)
{
    char buf[512];
    int s = (int64_t) arg;

    try {
	tcpconn tc(s);
	lineparser lp(&tc);

	const char *req = lp.read_line();
	if (req == 0 || strncmp(req, "GET ", 4))
	    throw basic_exception("bad http request: %s", req);

	const char *pn_start = req + 4;
	char *space = strchr(pn_start, ' ');
	if (space == 0)
	    throw basic_exception("no space in http req: %s", req);

	char pnbuf[256];
	strncpy(&pnbuf[0], pn_start, space - pn_start);
	pnbuf[sizeof(pnbuf) - 1] = '\0';

	char auth[64];
	auth[0] = '\0';
	while (req[0] != '\0') {
	    req = lp.read_line();
	    if (req == 0)
		throw basic_exception("client EOF");

	    const char *auth_prefix = "Authorization: Basic ";
	    if (!strncmp(req, auth_prefix, strlen(auth_prefix))) {
		strncpy(&auth[0], req + strlen(auth_prefix), sizeof(auth));
		auth[sizeof(auth) - 1] = '\0';
	    }
	}

	if (auth[0]) {
	    char *authdata = base64_decode(&auth[0]);
	    if (authdata == 0)
		throw error(-E_NO_MEM, "base64_decode");

	    char *colon = strchr(authdata, ':');
	    if (colon == 0)
		throw basic_exception("badly formatted authorization data");

	    *colon = 0;
	    char *user = authdata;
	    char *pass = colon + 1;

	    authd_reply reply;
	    if (auth_call(authd_login, user, pass, "", &reply) == 0) {
		label clear(2);
		clear.set(reply.user_taint, 3);
		error_check(sys_self_set_clearance(clear.to_ulabel()));

		label taint_label(LB_LEVEL_STAR);
		taint_label.set(reply.user_taint, 3);

		label untaint_label(3);
		untaint_label.set(reply.user_grant, LB_LEVEL_STAR);

		struct fs_inode worker_elf;
		error_check(fs_namei("/bin/httpd_worker", &worker_elf));

		int fds[2];
		error_check(pipe(fds));

		const char *argv[] = { "httpd_worker", &pnbuf[0] };
		const char *envv[] = {};

		spawn(start_env->shared_container, worker_elf,
		      0, fds[1], fds[1],
		      2, &argv[0],
		      0, &envv[0],
		      &taint_label, &untaint_label, 0, 0);

		close(fds[1]);

		for (;;) {
		    ssize_t cc = read(fds[0], buf, sizeof(buf));
		    if (cc < 0)
			throw error(cc, "reading from worker pipe");
		    if (cc == 0)
			return;
		    tc.write(buf, cc);
		}
	    }
	}

	snprintf(buf, sizeof(buf),
		"HTTP/1.0 401 Forbidden\r\n"
		"WWW-Authenticate: Basic realm=\"jos-httpd\"\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<h1>Please log in.</h1>\r\n");
	tc.write(buf, strlen(buf));
    } catch (std::exception &e) {
	printf("http_client: %s\n", e.what());
    }
}

static void __attribute__((noreturn))
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
	r = thread_create(start_env->proc_container, &http_client,
			  (void*) (int64_t) ss, &t, "http client");
	if (r < 0) {
	    printf("cannot spawn client thread: %s\n", e2s(r));
	    close(ss);
	} else {
	    fd_give_up_privilege(ss);
	}
    }
}

int
main(int ac, char **av)
{
    http_server();
}
