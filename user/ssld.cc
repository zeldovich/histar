extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/debug.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/ssld.h>
#include <inc/fd.h>
#include <inc/netd.h>
#include <inc/error.h>
#include <inc/string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <inc/memlayout.h>
#include <inc/taint.h>
#include <inc/stack.h>

#include <openssl/ssl.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

#include <map>

struct sock_data {
    BIO *io;
    SSL *ssl;
} ;

static struct cobj_ref ssld_gate_create(uint64_t ct);

std::map<int, struct sock_data> data;

static SSL_CTX *ctx;
static uint64_t access_grant;

static int 
password_cb(char *buf, int num, int rwflag, void *userdata)
{
    struct fs_inode ino;
    const char *pn = (const char *)userdata;
    error_check(fs_namei(pn, &ino));
    void *va = 0;
    uint64_t bytes = 0;
    error_check(segment_map(ino.obj, 0, SEGMAP_READ, &va, &bytes, 0));
    scope_guard<int, void *> unmap(segment_unmap, va);

    if(num < (int) (bytes + 1))
	return 0;
    
    memcpy(buf, va, bytes);
    buf[bytes] = 0;
    return strlen(buf);
}

static int
error_to_jos64(int ret)
{
    // XXX can SSL_get_error be used w/ BIO's failures?
    if (ret < 0)
	return -E_UNSPEC;
    return 0;
}

static int
ssld_accept(int lwip_sock, uint64_t netd_ct)
{
    int64_t gate_id = container_find(netd_ct, kobj_gate, "netd");
    if (gate_id < 0)
	return gate_id;
    struct cobj_ref netd_gate = COBJ(netd_ct, gate_id);
    netd_set_gate(netd_gate);

    struct Fd *fd;
    int r = fd_alloc(&fd, "socket fd");
    if (r < 0)
	return r;

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_sock.s = lwip_sock;
    fd->fd_sock.netd_gate = netd_gate;

    int s = fd2num(fd);
    
    BIO *sbio = BIO_new_socket(s, BIO_NOCLOSE);
    SSL *ssl = SSL_new(ctx);
    SSL_set_bio(ssl, sbio, sbio);
    
    if((r = SSL_accept(ssl)) <= 0)
	return -1;
    
    BIO *io = BIO_new(BIO_f_buffer());
    BIO *ssl_bio = BIO_new(BIO_f_ssl());
    BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);
    BIO_push(io, ssl_bio);
    
    data[lwip_sock].ssl = ssl;
    data[lwip_sock].io = io;

    return 0;
}

static int
ssld_send(BIO *io, void *buf, size_t count, int flags)
{
    int ret;
    if((ret = BIO_write(io, buf, count)) <= 0)
	return error_to_jos64(ret);
    
    int r;
    if((r = BIO_flush(io)) <= 0)
	return error_to_jos64(r);

    return ret;
}

static int
ssld_recv(SSL *ssl, BIO *io, void *buf, size_t count, int flags)
{
    int r = BIO_gets(io, (char *)buf, count);
    if (r <= 0)
	return error_to_jos64(r);
    return r;
}

static int
ssld_close(int lwip_sock)
{
    BIO_free(data[lwip_sock].io);
    SSL_shutdown(data[lwip_sock].ssl);
    SSL_free(data[lwip_sock].ssl);
    data.erase(lwip_sock);
    return 0;
}

void
ssld_dispatch(struct ssld_op_args *a)
{
    switch(a->op_type) {
    case ssld_op_accept:
	a->rval = ssld_accept(a->accept.s, a->accept.netd_ct);
	break;
    case ssld_op_send:
	a->rval = ssld_send(data[a->send.s].io, 
			    a->send.buf, a->send.count, a->send.flags);
	break;
    case ssld_op_recv:
	a->rval = ssld_recv(data[a->recv.s].ssl, data[a->recv.s].io, 
			    a->recv.buf, a->recv.count, a->recv.flags);
	break;
    case ssld_op_close:
	a->rval = ssld_close(a->close.s);
	break;
    }
}

static void __attribute__((noreturn))
ssld_gate(void *x, struct gate_call_data *gcd, gatesrv_return *gr)
{
    struct gate_call_data bck;
    gate_call_data_copy(&bck, gcd);

    uint64_t ssld_ct = start_env->proc_container;
    struct cobj_ref arg = gcd->param_obj;
    
    int64_t arg_copy_id = sys_segment_copy(arg, ssld_ct, 0,
					   "ssld_gate args");
    if (arg_copy_id < 0)
	panic("ssld_gate: cannot copy <%ld.%ld> args: %s",
	      arg.container, arg.object, e2s(arg_copy_id));
    sys_obj_unref(arg);
    
    struct cobj_ref arg_copy = COBJ(ssld_ct, arg_copy_id);
    struct ssld_op_args *a = 0;
    int r = segment_map(arg_copy, 0, SEGMAP_READ | SEGMAP_WRITE, (void**)&a, 0, 0);
    if (r < 0)
	panic("ssld_gate: cannot map args: %s\n", e2s(r));

    ssld_dispatch(a);
    gate_call_data_copy(gcd, &bck);

    segment_unmap(a);
    
    uint64_t copy_back_ct = gcd->taint_container;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct, 0,
					    "ssld_gate reply");
    if (copy_back_id < 0)
	panic("ssld_gate: cannot copy back: %s", e2s(copy_back_id));
    
    sys_obj_unref(arg_copy);
    gcd->param_obj = COBJ(copy_back_ct, copy_back_id);
    gr->ret(0, 0, 0);
}

static void __attribute__((noreturn))
ssld_cow_entry(void)
{
    try {
	// Copy-on-write if we are tainted
	gate_call_data *gcd = (gate_call_data *) TLS_GATE_ARGS;
	uint64_t *cow_ct = (uint64_t *)gcd->param_buf;
	taint_cow(*cow_ct, gcd->declassify_gate);

	// Reset our cached thread ID, stored in TLS
	if (tls_tidp)
	    *tls_tidp = sys_self_id();
	
	thread_label_cache_invalidate();
	
	int64_t *ret_id = (int64_t *)cow_ct;
	struct cobj_ref gate = ssld_gate_create(*cow_ct);
	*ret_id = gate.object;
	
	/*
	uint64_t entry_ct = start_env->proc_container;
	error_check(sys_self_set_sched_parents(gcd->taint_container, entry_ct));
	if (!(flags & GATESRV_NO_THREAD_ADDREF))
	    error_check(sys_self_addref(entry_ct));
	scope_guard<int, struct cobj_ref>
	    g(sys_obj_unref, COBJ(entry_ct, thread_id()));
	*/
	
	gatesrv_return ret(gcd->return_gate, start_env->proc_container,
			   gcd->taint_container, 0, GATESRV_KEEP_TLS_STACK);	
	ret.ret(0, 0, 0);
    } catch (std::exception &e) {
	printf("ssld_cow_entry: %s\n", e.what());
	thread_halt();
    }
}

static struct cobj_ref
ssld_cow_gate_create(uint64_t ct)
{
    label th_l, th_c;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_c);
    if (access_grant)
	th_c.set(access_grant, 0);
    
    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &ssld_cow_entry;
    te.te_stack = (char *) tls_stack_top - 8;
    error_check(sys_self_get_as(&te.te_as));

    int64_t gate_id = sys_gate_create(ct, &te,
				      th_c.to_ulabel(),
				      th_l.to_ulabel(), "ssld-cow", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    return COBJ(ct, gate_id);
}

static struct cobj_ref
ssld_gate_create(uint64_t ct)
{
    label th_l, th_c;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_c);
    if (access_grant)
	th_c.set(access_grant, 0);
    return gate_create(ct, "ssld", &th_l, &th_c, &ssld_gate, 0);
}

void
ssl_init(const char *server_pem, const char *password, 
	 const char *dh_pem, const char *calist_pem)
{
    if (ctx) {
	SSL_CTX_free(ctx);
	ctx = 0;
    }

    SSL_library_init();
    SSL_load_error_strings();

    SSL_METHOD *meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    // Load our keys and certificates
    if(!(SSL_CTX_use_certificate_chain_file(ctx, server_pem)))
	throw basic_exception("Can't read certificate file %s", server_pem);
    
    char *pass = strdup(password);
    scope_guard<void, char *> g(delete_obj, pass);
    
    SSL_CTX_set_default_passwd_cb_userdata(ctx, pass);
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
    
    if(!(SSL_CTX_use_PrivateKey_file(ctx, server_pem, SSL_FILETYPE_PEM)))
	throw basic_exception("Can't read key file %s", server_pem);

    // Load the CAs we trust
    if (calist_pem)
	if(!(SSL_CTX_load_verify_locations(ctx, calist_pem, 0)))
	    throw basic_exception("Can't read CA list %s", calist_pem);
    
    // From an example
    if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);

    // Load the dh params to use
    if (dh_pem) {
	DH *ret = 0;
	BIO *bio;
	
	if (!(bio = BIO_new_file(dh_pem,"r")))
	    throw basic_exception("Couldn't open DH file %s", dh_pem);
	
	ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if(SSL_CTX_set_tmp_dh(ctx, ret) < 0 )
	    throw basic_exception("Couldn't set DH parameters using %s", dh_pem);
    }
}

int
main (int ac, char **av)
{
    if (ac < 5) {
	cprintf("Usage: %s server-pem password dh-pem access-grant", 
		av[0]);
	return -1;
    }

    const char *server_pem = av[1];
    const char *password = av[2];
    const char *dh_pem = av[3];
    uint64_t access_grant;
    error_check(strtou64(av[4], 0, 10, &access_grant));

    netd_init_client();
    ssl_init(server_pem, password, dh_pem, 0);

    // only create a cow gate initially
    ssld_cow_gate_create(start_env->shared_container);
        
    return 0;
}
