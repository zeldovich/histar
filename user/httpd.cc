extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/error.h>
}
#include <inc/nethelper.hh>

static int reqs;

static void
http_client(void *arg)
{
    int s = (int64_t) arg;

    try {
	tcpconn tc(s);
	lineparser lp(&tc);

	const char *req = lp.read_line();
	if (req == 0 || strncmp(req, "GET ", 4))
	    throw errormsg("bad http request: %s", req);

	const char *pn_start = req + 4;
	char *space = strchr(pn_start, ' ');
	if (space == 0)
	    throw errormsg("no space in http req: %s", req);

	char pnbuf[512];
	strncpy(&pnbuf[0], pn_start, space - pn_start);
	pnbuf[sizeof(pnbuf) - 1] = '\0';

	while (req[0] != '\0') {
	    req = lp.read_line();
	    if (req == 0)
		throw errormsg("client EOF");
	}

	char buf[1024];
	snprintf(buf, sizeof(buf),
		"HTTP/1.0 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<h1>jos64 web server: request %d</h1><p><hr><pre>\n",
		reqs++);
	tc.write(buf, strlen(buf));

	try {
	    snprintf(buf, sizeof(buf), "Trying to lookup %s\n", pnbuf);
	    tc.write(buf, strlen(buf));

	    struct fs_inode ino;
	    int r = fs_namei(pnbuf, &ino);
	    if (r < 0)
		throw errormsg("fs_namei: %s", e2s(r));

	    int type = sys_obj_get_type(ino.obj);
	    if (type < 0)
		throw errormsg("sys_obj_get_type: %s", e2s(r));

	    if (type == kobj_segment) {
		snprintf(buf, sizeof(buf), "segment\n");
		tc.write(buf, strlen(buf));

		uint64_t sz;
		r = fs_getsize(ino, &sz);
		if (r < 0)
		    throw errormsg("fs_getsize: %s", e2s(r));

		for (uint64_t off = 0; off < sz; off += sizeof(buf)) {
		    r = fs_pread(ino, &buf[0], sizeof(buf), off);
		    if (r < 0)
			throw errormsg("fs_pread: %s", e2s(r));
		    tc.write(buf, sizeof(buf));
		}
	    } else if (type == kobj_container || type == kobj_mlt) {
		snprintf(buf, sizeof(buf), "directory or mlt\n");
		tc.write(buf, strlen(buf));

		struct fs_dent de;
		for (uint64_t n = 0; ; n++) {
		    r = fs_get_dent(ino, n, &de);
		    if (r == -E_RANGE)
			break;
		    if (r == -E_NOT_FOUND)
			continue;
		    if (r < 0)
			throw errormsg("fs_get_dent: %s", e2s(r));

		    snprintf(buf, sizeof(buf),
			     "<a href=\"%s%s%s\">%s/%s</a>\n",
			     pnbuf,
			     pnbuf[strlen(pnbuf) - 1] == '/' ? "" : "/",
			     &de.de_name[0],
			     pnbuf, &de.de_name[0]);
		    tc.write(buf, strlen(buf));
		}
	    } else {
		snprintf(buf, sizeof(buf), "type %d\n", type);
		tc.write(buf, strlen(buf));
	    }
	} catch (std::exception &e) {
	    const char *what = e.what();
	    tc.write(what, strlen(what));
	}
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
	r = thread_create(start_env->container, &http_client,
			  (void*) (int64_t) ss, &t, "http client");
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
