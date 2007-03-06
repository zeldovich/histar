extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
}

#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/nethelper.hh>
#include <dj/gatesender.hh>
#include <dj/djops.hh>

// for 'server' mode
static strbuf gs_key;
static strbuf callct;
static strbuf authgate;
static strbuf fsgate;
static strbuf appgate;

static void
write_to_file(const char *pn, const strbuf &sb)
{
    int fd = open(pn, O_RDWR | O_CREAT, 0666);
    errno_check(fd);

    str s(sb);
    write(fd, s.cstr(), s.len());
    close(fd);
}

static void
write_to_tcpconn(tcpconn &tc, const char *name, strbuf sb)
{
    char buf[128];
    str s(sb);
    
    strncpy(buf, name, sizeof(buf));
    strcat(buf, "\r\n");

    tc.write(buf, strlen(buf));
    snprintf(buf, sizeof(buf), "%lu\r\n", s.len());
    tc.write(buf, strlen(buf));
    tc.write(s.cstr(), s.len());
}

static void
server(void)
{
    static uint16_t port = 1234;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("server on port %d\n", port);
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }
	printf("client connected...\n");

	tcpconn tc(ss, 0);
	lineparser lp(&tc);
	
	const char *req;
	if ((req = lp.read_line())) {
	    printf("req %s\n", req);
	    if (!strcmp(req, "stop"))
		return;
	    else if(!strcmp(req, "user")) {
		write_to_tcpconn(tc, "dj_user_host", gs_key);
		write_to_tcpconn(tc, "dj_user_ct", callct);
		write_to_tcpconn(tc, "dj_user_authgate", authgate);
		write_to_tcpconn(tc, "dj_user_fsgate", fsgate);
	    } else if (!strcmp(req, "app")) {
		write_to_tcpconn(tc, "dj_app_host", gs_key);
		write_to_tcpconn(tc, "dj_app_ct", callct);
		write_to_tcpconn(tc, "dj_app_gate", appgate);
	    }
	}
	close(ss);
    }
}

int
main(int ac, char **av)
{
    gate_sender gs;

    gs_key << gs.hostkey();

    int64_t ct, id;
    error_check(ct = container_find(start_env->root_container, kobj_container, "djechod"));
    error_check(id = container_find(ct, kobj_container, "public call"));
    callct << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djauthproxy"));
    error_check(id = container_find(ct, kobj_gate, "authproxy"));
    authgate << ct << "." << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djwebappd"));
    error_check(id = container_find(ct, kobj_gate, "djwebappd"));
    appgate << ct << "." << id << "\n";

    error_check(ct = container_find(start_env->root_container, kobj_container, "djfsd"));
    error_check(id = container_find(ct, kobj_gate, "djfsd"));
    fsgate << ct << "." << id << "\n";

    write_to_file("/www/dj_user_host", gs_key);
    write_to_file("/www/dj_app_host", gs_key);
    write_to_file("/www/dj_user_ct", callct);
    write_to_file("/www/dj_app_ct", callct);
    write_to_file("/www/dj_user_authgate", authgate);
    write_to_file("/www/dj_user_fsgate", fsgate);
    write_to_file("/www/dj_app_gate", appgate);

    system("adduser alice foo");
    system("adduser bob bar");
    warn << "All done, alice/foo, bob/bar\n";
    
    if (ac >= 2 && av[1][0] == 's') {
	warn << "starting server...\n";
	server();
    }
}
