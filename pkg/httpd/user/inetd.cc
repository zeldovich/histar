extern "C" {
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/bipipe.h>
#include <inc/debug.h>
#include <inc/argv.h>
#include <inc/fd.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <inc/spawn.hh>
#include <inc/sslproxy.hh>
#include <inc/scopeguard.hh>

static char ssl_privsep_enable;
static char ssl_eproc_enable;
static char http_auth_enable;
static char http_dj_enable;
static fs_inode httpd_root_ino;
static char *httpd_path;
static cobj_ref httpd_mtab_seg;

arg_desc cmdarg[] = {
    { "ssl_privsep_enable", "1" },
    { "ssl_eproc_enable", "1" },
    
    { "http_auth_enable", "0" },
    { "http_dj_enable", "0" },
    { "httpd_path", "/bin/httpd2"},
    { "httpd_root_path", "/www" },
        
    { 0, "" }
};

static struct cobj_ref the_ssld_cow;
static struct cobj_ref the_eprocd_cow;

union conn_info {
    struct {
	int sock;
	uint32_t count;
    } data;
    uint64_t u64;
};

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
    int r = fs_namei("/djechod/public call/ssl_eprocd", &ct_ino);
    if (r < 0)
	error_check(fs_namei("/httpd/ssl_eprocd", &ct_ino));
    uint64_t eproc_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(eproc_ct, kobj_gate, "eproc-cow"));
    return COBJ(eproc_ct, gate_id);
}

static void
spawn_httpd(uint64_t ct, jcomm_ref plain_comm, uint64_t taint, uint32_t count)
{
    char container_arg[32], object_arg[32], chan_arg[32], count_arg[32];
    snprintf(container_arg, sizeof(container_arg), 
	     "%lu", plain_comm.container);
    snprintf(object_arg, sizeof(object_arg), 
	     "%lu", plain_comm.jc.segment);
    snprintf(chan_arg, sizeof(chan_arg), 
	     "%d", plain_comm.jc.chan);
    snprintf(count_arg, sizeof(count_arg), 
	     "%d", count);

    fs_inode ino;
    error_check(fs_namei(httpd_path, &ino));
    const char *argv[] = { httpd_path, 
			   container_arg, 
			   object_arg, 
			   chan_arg,
			   http_auth_enable ? "1" : "0",
			   count_arg,
			   http_dj_enable ? "1" : "0" };

    label ds(3), dr(1);
    ds.set(taint, LB_LEVEL_STAR);
    dr.set(taint, 3);

    spawn_descriptor sd;
    sd.ct_ = ct;
    sd.elf_ino_ = ino;
    sd.fd0_ = 0;
    sd.fd1_ = 0;
    sd.fd2_ = 0;
    
    sd.ac_ = sizeof(argv) / sizeof(argv[0]);
    sd.av_ = &argv[0];

    sd.ds_ = &ds;
    sd.dr_ = &dr;
    
    sd.fs_mtab_seg_ = httpd_mtab_seg;
    sd.fs_root_ = httpd_root_ino;
    sd.fs_cwd_ = httpd_root_ino;
    spawn(&sd);

}

static void
inet_client(void *a)
{
    conn_info ci;
    ci.u64 = (uint64_t)a;
    int ss = ci.data.sock;
    try {
	ssl_proxy_descriptor d;
	ssl_proxy_alloc(the_ssld_cow, the_eprocd_cow, 
			start_env->shared_container, ss, &d);
	
	// we don't care about waiting for http2 to finish
	// with the connection, so don't use ssl_proxy_client*
	struct ssl_proxy_client *spc = 0;
	uint64_t bytes = sizeof(*spc);
	error_check(segment_map(d.client_seg_, 0, SEGMAP_READ, 
				(void **)&spc, &bytes, 0));
	scope_guard2<int, void *, int> spc_cu(segment_unmap_delayed, spc, 0);	
	
	spawn_httpd(d.ssl_ct_, spc->plain_comm_, d.taint_, ci.data.count);
	ssl_proxy_loop(&d, 1);
    } catch (basic_exception &e) {
	printf("inet_client: %s\n", e.what());
    }	
}

static void __attribute__((noreturn))
inet_server(uint16_t port)
{
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

    r = listen(s, 10);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    printf("inetd: server on port %d\n", port);
    for (uint32_t i = 0;; i++) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0)
            panic("cannot accept client: %s\n", strerror(ss));
	
	conn_info ci;
	ci.data.sock = ss;
	ci.data.count = i;
	struct cobj_ref t;
	r = thread_create(start_env->proc_container, &inet_client,
			  (void*) ci.u64, &t, "inet client");

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
    http_dj_enable = atoi(arg_val(cmdarg, "http_dj_enable"));
    httpd_path = arg_val(cmdarg, "httpd_path");

    const char * httpd_root_path = arg_val(cmdarg, "httpd_root_path");
    error_check(fs_namei(httpd_root_path, &httpd_root_ino));

    // setup mount table for httpd
    int64_t new_mtab_id;
    error_check(new_mtab_id = 
		sys_segment_copy(start_env->fs_mtab_seg, 
				 start_env->shared_container,
				 0, "http mtab"));
    httpd_mtab_seg = COBJ(start_env->shared_container, new_mtab_id);
    fs_inode home_dir, etc_dir, share_dir, bin_dir, dev_dir;
    error_check(fs_namei("/home", &home_dir));
    error_check(fs_namei("/etc", &etc_dir));
    error_check(fs_namei("/share", &share_dir));
    error_check(fs_namei("/bin", &bin_dir));
    error_check(fs_namei("/dev", &dev_dir));
    error_check(fs_mount(httpd_mtab_seg, httpd_root_ino, "home", home_dir));
    error_check(fs_mount(httpd_mtab_seg, httpd_root_ino, "etc", etc_dir));
    error_check(fs_mount(httpd_mtab_seg, httpd_root_ino, "share", share_dir));
    error_check(fs_mount(httpd_mtab_seg, httpd_root_ino, "bin", bin_dir));
    error_check(fs_mount(httpd_mtab_seg, httpd_root_ino, "dev", dev_dir));
    
    printf("inted: config:\n");
    printf(" %-20s %d\n", "ssl_privsep_enable", ssl_privsep_enable);
    printf(" %-20s %d\n", "ssl_eproc_enable", ssl_eproc_enable);
    printf(" %-20s %d\n", "http_auth_enable", http_auth_enable);
    printf(" %-20s %d\n", "http_dj_enable", http_dj_enable);
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
    inet_server(443);
}
