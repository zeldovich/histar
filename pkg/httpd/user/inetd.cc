extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/bipipe.h>
#include <inc/debug.h>
#include <inc/ssl_fd.h>
#include <inc/argv.h>

#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/spawn.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/ssldclnt.hh>
#include <inc/sslproxy.hh>

#include <inc/a2pdf.hh>
#include <inc/perl.hh>
#include <inc/wrap.hh>

#include <iostream>
#include <sstream>

static char ssl_privsep_enable;
static char ssl_eproc_enable;
static char http_auth_enable;
static fs_inode httpd_root_ino;
static char *httpd_path;

arg_desc cmdarg[] = {
    { "ssl_enable", "1" },
    { "ssl_privsep_enable", "1" },
    { "ssl_eproc_enable", "1" },
    
    { "ssl_server_pem", "/bin/server.pem" },
    { "ssl_dh_pem", "/bin/dh.pem" },
    { "ssl_servkey_pem", "/bin/servkey.pem" },

    { "http_auth_enable", "0" },
    { "httpd_path", "/bin/httpd2"},
    { "httpd_root_path", "/www" },
        
    { 0, 0 }
};

static struct cobj_ref the_ssld_cow;
static struct cobj_ref the_eprocd_cow;

static struct cobj_ref
get_ssld_cow(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/httpd/ssld/", &ct_ino));
    uint64_t ssld_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(ssld_ct, kobj_gate, "ssld-cow"));
    return COBJ(ssld_ct, gate_id);
}

static struct cobj_ref
get_eprocd_cow(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/httpd/ssl_eprocd/", &ct_ino));
    uint64_t eproc_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(eproc_ct, kobj_gate, "eproc-cow"));
    return COBJ(eproc_ct, gate_id);
}

static void
spawn_httpd(uint64_t ct, cobj_ref plain_bipipe, uint64_t taint)
{
    char container_arg[32], object_arg[32];
    snprintf(container_arg, sizeof(container_arg), 
	     "%lu", plain_bipipe.container);
    snprintf(object_arg, sizeof(object_arg), 
	     "%lu", plain_bipipe.object);

    fs_inode ino;
    error_check(fs_namei(httpd_path, &ino));
    const char *argv[] = { httpd_path, container_arg, object_arg };

    label cs(LB_LEVEL_STAR), dr(1), co(0);
    cs.set(taint, 3);
    dr.set(taint, 3);
    co.set(taint, 3);

    spawn(ct, ino,
	  0, 0, 0,
	  3, &argv[0],
	  0, 0,
	  &cs, 0, 0, &dr, &co);
}

static void
inet_client(void *a)
{
    int ss = (int64_t) a;
    try {
	ssl_proxy_descriptor d;
	ssl_proxy_alloc(the_ssld_cow, the_eprocd_cow, 
			start_env->shared_container, ss, &d);
	spawn_httpd(d.ssl_ct_, d.plain_bipipe_, d.taint_);
	ssl_proxy_loop(&d, 1);
    } catch (basic_exception &e) {
	printf("inet_client: %s\n", e.what());
    }	
}


static void __attribute__((noreturn))
inet_server(void)
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

    printf("inetd: http server on port 80\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	struct cobj_ref t;
	r = thread_create(start_env->proc_container, &inet_client,
			  (void*) (int64_t) ss, &t, "inet client");

	if (r < 0) {
	    printf("cannot spawn client thread: %s\n", e2s(r));
	    close(ss);
	} else {
	    fd_give_up_privilege(ss);
	}	
    }
}

int
main(int ac, const char **av)
{
    error_check(argv_parse(ac, av, cmdarg));

    ssl_privsep_enable = atoi(arg_val(cmdarg, "ssl_privsep_enable"));
    ssl_eproc_enable = atoi(arg_val(cmdarg, "ssl_eproc_enable"));
    http_auth_enable = atoi(arg_val(cmdarg, "http_auth_enable"));
    httpd_path = arg_val(cmdarg, "httpd_path");

    const char * httpd_root_path = arg_val(cmdarg, "httpd_root_path");
    error_check(fs_namei(httpd_root_path, &httpd_root_ino));

    printf("inted: config:\n");
    printf(" %-20s %d\n", "ssl_privsep_enable", ssl_privsep_enable);
    printf(" %-20s %d\n", "ssl_eproc_enable", ssl_eproc_enable);
    printf(" %-20s %d\n", "http_auth_enable", http_auth_enable);
    printf(" %-20s %s\n", "httpd_root_path", httpd_root_path);
    printf(" %-20s %s\n", "httpd_path", httpd_path);

    if (ssl_privsep_enable) {
	the_ssld_cow = get_ssld_cow();
	try {
	    the_eprocd_cow = get_eprocd_cow();
	} catch (error &e) {
	    if (e.err() == -E_NOT_FOUND) {
		printf("httpd: unable to find eprocd\n");
		the_eprocd_cow = COBJ(0, 0);
	    }
	    else {
		printf("httpd: %s\n", e.what());
		return -1;
	    }
	}
    } 
    inet_server();
}
