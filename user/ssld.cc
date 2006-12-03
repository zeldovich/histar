extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/debug.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/fd.h>
#include <inc/netd.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/bipipe.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <inc/memlayout.h>
#include <inc/taint.h>
#include <inc/stack.h>

#include <openssl/ssl.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/ssldclnt.hh>

static const char dbg = 1;

static SSL_CTX *ctx;
static uint64_t access_grant;

static struct cobj_ref cipher_biseg;
static struct cobj_ref plain_biseg;

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
ssld_accept(int cipher_sock, SSL **ret)
{
    int s = cipher_sock;
    
    BIO *sbio = BIO_new_socket(s, BIO_NOCLOSE);
    SSL *ssl = SSL_new(ctx);
    SSL_set_bio(ssl, sbio, sbio);
    
    int r;
    if((r = SSL_accept(ssl)) <= 0)
	return -1;
    
    *ret = ssl;
    return 0;
}

static int
ssld_send(SSL *ssl, void *buf, size_t count, int flags)
{
    int r = SSL_write(ssl, (char *)buf, count);
    if (r <= 0)
	return error_to_jos64(r);
    return r;
    
}

static int
ssld_recv(SSL *ssl, void *buf, size_t count, int flags)
{
    int r = SSL_read(ssl, (char *)buf, count);
    if (r <= 0)
	return error_to_jos64(r);
    return r;
}


static int
ssld_close(SSL *ssl)
{
    // don't do two-step shutdown procedure
    SSL_shutdown(ssl);
    SSL_free(ssl);
    return 0;
}

static void
ssld_worker(void *arg)
{
    try {
	int cipher_fd = bipipe_fd(cipher_biseg, 1, 0);
	int plain_fd = bipipe_fd(plain_biseg, 1, 0);
	error_check(cipher_fd);
	error_check(plain_fd);
	
	SSL *ssl = 0;
	int r = ssld_accept(cipher_fd, &ssl);
	if (r < 0) {
	    cprintf("ssld_worker: unable to accept SSL connection\n");
	    close(cipher_fd);
	    close(plain_fd);
	    thread_halt();
	}

	debug_cprint(dbg, "SSL connection established");
	
	char buf[4096];
	fd_set readset, writeset, exceptset;
	int maxfd = MAX(cipher_fd, plain_fd) + 1;
	
	for (;;) {
	    FD_ZERO(&readset);
	    FD_ZERO(&writeset);
	    FD_ZERO(&exceptset);
	    FD_SET(plain_fd, &readset);	
	    FD_SET(cipher_fd, &readset);	
	    
	    int r = select(maxfd, &readset, &writeset, &exceptset, 0);
	    if (r < 0)
		throw basic_exception("unknown select error: %s\n", 
				      strerror(errno));

	    if (FD_ISSET(plain_fd, &readset)) {
		int r1 = read(plain_fd, buf, sizeof(buf));
		if (!r1) {
		    // other end of plan_fd closed
		    debug_cprint(dbg, "stopping -- plain fd closed");
		    close(plain_fd);
		    ssld_close(ssl);
		    close(cipher_fd);
		    return;
		} else if (r1 < 0) {
		    cprintf("unknown read error: %d\n", r1);
		} else {
		    int r2 = ssld_send(ssl, buf, r1, 0);
		    // XXX
		    assert(r1 == r2);
		}
	    }
	    if (FD_ISSET(cipher_fd, &readset)) {
		int r1 = ssld_recv(ssl, buf, sizeof(buf), 0);
		if (r1 < 0) {
		    cprintf("unknown recv error: %d\n", r1);
		} else {
		    int r2 = write(plain_fd, buf, r1);
		    if (r2 < 0) {
			if (errno == EPIPE) {
			    // other end of plan_fd closed
			    debug_cprint(dbg, "stopping -- plain fd closed");
			    close(plain_fd);
			    ssld_close(ssl);
			    close(cipher_fd);
			    return;
			}
			else {
			    cprintf("unknown write error: %d\n", r2);
			}
		    }    
		    // XXX
		    assert(r1 == r2);
		}
	    }
	}    
    } catch (std::exception &e) {
	cprintf("ssld_worker: %s\n", e.what());
	thread_halt();
    }
}

static void __attribute__((noreturn))
ssld_cow_entry(void)
{
    try {
	// Copy-on-write if we are tainted
	gate_call_data *gcd = (gate_call_data *) TLS_GATE_ARGS;
	struct ssld_cow_op *op = (struct ssld_cow_op *)gcd->param_buf;

	uint64_t cow_ct = op->root_ct;
	if (!taint_cow(cow_ct, gcd->declassify_gate))
	    throw error(-E_UNSPEC, "cow didn't happen?");

	if (tls_tidp)
	    *tls_tidp = sys_self_id();
	
	thread_label_cache_invalidate();
		
	cipher_biseg = op->cipher_biseg;
	plain_biseg = op->plain_biseg;

	struct cobj_ref t;
	int r = thread_create(start_env->proc_container, &ssld_worker,
			      0, &t, "ssld-worker");
	if (r < 0)
	    throw error(r, "unable to spawn worker: %s\n", e2s(r));

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
	cprintf("ssld_cow_entry: %s\n", e.what());
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

    ssl_init(server_pem, password, dh_pem, 0);
    ssld_cow_gate_create(start_env->shared_container);
        
    return 0;
}
