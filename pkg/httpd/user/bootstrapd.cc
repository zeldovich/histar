extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
#include <inc/error.h>
}

#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/nethelper.hh>
#include <inc/spawn.hh>
#include <dj/gatesender.hh>
#include <dj/djops.hh>

// for 'server' mode
static strbuf gs_key;
static strbuf callct;
static strbuf authgate;
static strbuf fsgate;
static strbuf appgate;

extern const char *jos_progname;

static void
untar(const char *in_pn, const char *tar_fn)
{
    static const char *tar_pn = "/bin/tar";
    fs_inode ino;
    error_check(fs_namei(tar_pn, &ino));
    const char *argv[] = { tar_pn, "xm", "-C", in_pn, "-f", tar_fn };
    
    spawn_descriptor sd;
    sd.ct_ = start_env->shared_container;
    sd.elf_ino_ = ino;
    sd.ac_ = 6;
    sd.av_ = &argv[0];

    struct child_process cp = spawn(&sd);
    int64_t exit_code;
    error_check(process_wait(&cp, &exit_code));
    error_check(exit_code);
}

static void
create_ascii(const char *dn, const char *fn, uint64_t len)
{
    struct fs_inode dir;
    error_check(fs_namei(dn, &dir));
    struct fs_inode file;
    error_check(fs_create(dir, fn, &file, 0));

    void *buf = malloc(len);
    memset(buf, 'a', len);
    int r = fs_pwrite(file, buf, len, 0);
    free(buf);
    error_check(r);
}

static void
write_to_file(const char *pn, const strbuf &sb)
{
    int fd = open(pn, O_RDWR | O_CREAT | O_TRUNC, 0666);
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

    warn << jos_progname << ": server on port " << port << "\n";
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
	    warn << jos_progname << ": cannot accept client: " << ss << "\n";
            continue;
        }
	warn << jos_progname << ": client connected...\n";

	tcpconn tc(ss, 0);
	lineparser lp(&tc);
	
	const char *req;
	if ((req = lp.read_line())) {
	    warn << jos_progname << ": req " << req << "\n";
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

static void
init(void)
{
    mkdir("/www", 0);

    for (int retry = 0; ; retry++) {
	strbuf _callct;
	strbuf _authgate;
	strbuf _fsgate;
	strbuf _appgate;
	strbuf _gs_key;
	
	try {
	    gate_sender gs;
	    _gs_key << gs.hostkey();
	    
	    int64_t ct, id;
	    error_check(ct = container_find(start_env->root_container, kobj_container, "djechod"));
	    error_check(id = container_find(ct, kobj_container, "public call"));
	    _callct << id << "\n";
	    
	    error_check(ct = container_find(start_env->root_container, kobj_container, "djauthproxy"));
	    error_check(id = container_find(ct, kobj_gate, "authproxy"));
	    _authgate << ct << "." << id << "\n";

	    error_check(ct = container_find(start_env->root_container, kobj_container, "djwebappd"));
	    error_check(id = container_find(ct, kobj_gate, "djwebappd"));
	    _appgate << ct << "." << id << "\n";
	    
	    error_check(ct = container_find(start_env->root_container, kobj_container, "djfsd"));
	    error_check(id = container_find(ct, kobj_gate, "djfsd"));
	    _fsgate << ct << "." << id << "\n";
	} catch (basic_exception &e) {
	    if (retry >= 100) {
		fprintf(stderr, "%s: ERROR: timed out waiting for dj helpers\n", 
			jos_progname);
		throw e;
	    }
	    sleep(1);
	    continue;
	}
	
	callct << _callct;
	authgate << _authgate;
	fsgate << _fsgate;
	appgate << _appgate;
	gs_key << _gs_key;
	break;
    }

    for (int retry = 0; ; retry++) {
	try {
	    write_to_file("/www/dj_user_host0", gs_key);
	    write_to_file("/www/dj_app_host0", gs_key);
	    write_to_file("/www/dj_user_ct0", callct);
	    write_to_file("/www/dj_app_ct0", callct);
	    write_to_file("/www/dj_user_authgate0", authgate);
	    write_to_file("/www/dj_user_fsgate0", fsgate);
	    write_to_file("/www/dj_app_gate0", appgate);
	} catch (basic_exception &e) {
	    if (retry >= 100) {
		fprintf(stderr, "%s: ERROR: timed out trying to write bootstrap files\n",
			jos_progname);
		throw e;
	    }
	    sleep(1);
	    continue;
	}
	break;
    }

    try {
	untar("/", "/bin/a2ps.tar");
	untar("/", "/bin/gs.tar");
    } catch (basic_exception &e) {
	fprintf(stderr, "%s: WARNING: cannot untar apps: %s\n", 
		jos_progname, e.what());
    }
    
    try {
	create_ascii("/www/", "test.0", 0);
	create_ascii("/www/", "test.1", 1);
	create_ascii("/www/", "test.1024", 1024);
	create_ascii("/www/", "test.8192", 8192);
    } catch(basic_exception &e) {
	fprintf(stderr, "%s: WARNING: cannot create test files: %s\n", 
		jos_progname, e.what());
    }

    system("adduser alice foo");
    system("adduser bob bar");
    warn << jos_progname << ": All done, alice/foo, bob/bar\n";
}

int
main(int ac, char **av)
{
    init();
    
    if (ac >= 2 && av[1][0] == 'x') {
	warn << "not starting server...\n";
	return 0;
    }
    
    warn << jos_progname << ": starting server...\n";
    server();
    return 0;
}
