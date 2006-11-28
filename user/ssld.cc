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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/ssl.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

#include <map>

struct sock_data {
    BIO *io;
    SSL *ssl;
} ;

std::map<int, struct sock_data> data;

SSL_CTX *ctx;

// XXX
static const char *pass;

static int password_cb(char *buf, int num, int rwflag, void *userdata)
{
    if(num < (int) strlen(pass) + 1)
	return 0;
    
    strcpy(buf, pass);
    return strlen(pass);
}

static int
error_to_jos64(int ret)
{
    // XXX can SSL_get_error be used w/ BIO's failures?
    if (ret < 0)
	return -E_UNSPEC;
    return 0;
}


void
init_ssl(const char *server_pem, const char *calist_pem, const char *dh_pem)
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
	throw basic_exception("Can't read certificate file");
    
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
    
    if(!(SSL_CTX_use_PrivateKey_file(ctx, server_pem, SSL_FILETYPE_PEM)))
	throw basic_exception("Can't read key file");

    // Load the CAs we trust
    if(!(SSL_CTX_load_verify_locations(ctx, calist_pem, 0)))
	throw basic_exception("Can't read CA list");
    
    // From an example
    if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);

    // Load the dh params to use
    DH *ret = 0;
    BIO *bio;
    
    if (!(bio = BIO_new_file(dh_pem,"r")))
	throw basic_exception("Couldn't open DH file");
    
    ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if(SSL_CTX_set_tmp_dh(ctx, ret) < 0 )
	throw basic_exception("Couldn't set DH parameters");
}

static int
ssld_socket(int lwip_sock, struct cobj_ref netd_gate)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "socket fd");
    if (r < 0)
	return r;

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_sock.s = lwip_sock;
    fd->fd_sock.netd_gate = netd_get_gate();

    int s = fd2num(fd);
    
    // XXX what needs to be dealloced on close?
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
    case ssld_op_socket:
	a->rval = ssld_socket(a->socket.s, a->socket.netd_gate);
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
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

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

int
main (int ac, char **av)
{
    if (ac != 5) {
	cprintf("Usage: %s server-pem password calist-pem dh-pem", av[0]);
	return -1;
    }

    pass = av[2];

    netd_init_client();
    init_ssl(av[1], av[3], av[4]);

    label th_l, th_c;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_c);
    
    gate_create(start_env->shared_container, "ssld",
		&th_l, &th_c, &ssld_gate, 0);
    
    return 0;
}
