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
#include <inc/openssl.h>

#include <openssl/ssl.h>
#include <openssl/engine.h>
}

#include <inc/sslproxy.hh>
#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/ssldclnt.hh>

static const char dbg = 0;

static SSL_CTX *the_ctx;

static char *cow_stacktop;

static int
ssld_accept(int cipher_sock, SSL **ret)
{
    int s = cipher_sock;
    
    BIO *sbio = BIO_new_socket(s, BIO_NOCLOSE);
    SSL *ssl = SSL_new(the_ctx);
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
    if (r <= 0) {
	openssl_print_error(ssl, r, 1);
	return -1;
    }
    return r;
    
}

static int
ssld_recv(SSL *ssl, void *buf, size_t count, int flags)
{
    int r = SSL_read(ssl, (char *)buf, count);
    if (r <= 0) {
	openssl_print_error(ssl, r, 1);
	return -1;
    }
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

static void __attribute__((noreturn))
ssld_worker(uint64_t cc, uint64_t co, uint64_t pc, uint64_t po)
{
    debug_cprint(dbg, "starting...");
    SSL *ssl = 0;

    // don't worry about extra taint and grant
    int cipher_fd = bipipe_fd(COBJ(cc, co), ssl_proxy_bipipe_ssld, 0, 0, 0);
    int plain_fd = bipipe_fd(COBJ(pc, po), ssl_proxy_bipipe_ssld, 0, 0, 0);
    error_check(cipher_fd);
    error_check(plain_fd);
    
    try {
	int r = ssld_accept(cipher_fd, &ssl);
	if (r < 0) {
	    cprintf("ssld_worker: unable to accept SSL connection\n");
	    close(cipher_fd);
	    close(plain_fd);
	    thread_halt();
	}

	debug_cprint(dbg, "SSL connection established");
	
	char buf[4096];
	fd_set readset, writeset;
	int maxfd = MAX(cipher_fd, plain_fd) + 1;
	
	for (;;) {
	    FD_ZERO(&readset);
	    FD_ZERO(&writeset);
	    FD_SET(plain_fd, &readset);	
	    FD_SET(cipher_fd, &readset);	
	    
	    int r = select(maxfd, &readset, &writeset, 0, 0);
	    if (r < 0)
		throw basic_exception("unknown select error: %s\n", 
				      strerror(errno));

	    if (FD_ISSET(plain_fd, &readset)) {
		int r1 = read(plain_fd, buf, sizeof(buf));
		if (!r1) {
		    // other end of plan_fd closed
		    debug_cprint(dbg, "stopping -- plain fd closed");
		    break;
		} else if (r1 < 0) {
		    throw basic_exception("plain_fd read error %s", 
					  strerror(errno));
		} else {
		    int r2 = ssld_send(ssl, buf, r1, 0);
		    if (r2 != r1)
			throw basic_exception("ssld_send error %d, %d", r1, r2);
		}
	    }
	    if (FD_ISSET(cipher_fd, &readset)) {
		int r1 = ssld_recv(ssl, buf, sizeof(buf), 0);
		if (r1 < 0) {
		    throw basic_exception("ssld_recv error %d", r1); 
		} else if (!r1) {
		    debug_cprint(dbg, "stopping -- cipher fd closed");
		    break;
		} else {
		    int r2 = write(plain_fd, buf, r1);
		    if (r2 < 0) {
			if (errno == EPIPE) {
			    // other end of plan_fd closed
			    debug_cprint(dbg, "stopping -- plain fd closed");
			    break;
			}
			else {
			    throw basic_exception("plain_fd write error %s",
						  strerror(errno));
			}
		    }    
		    if (r1 != r2)
			throw basic_exception("plain_fd write error %d, %d", 
					      r1, r2);
		}
	    }
	}    
    } catch (std::exception &e) {
	cprintf("ssld_worker: %s\n", e.what());
    }

    close(plain_fd);
    ssld_close(ssl);
    close(cipher_fd);
    thread_halt();
}

static void
ssl_load_file_privkey(SSL_CTX *ctx, const char *servkey_pem)
{
    if(!(SSL_CTX_use_PrivateKey_file(ctx, servkey_pem, SSL_FILETYPE_PEM)))
	throw basic_exception("Can't read key file %s", servkey_pem);    
}

static void
ssl_load_rmt_privkey(SSL_CTX *ctx, struct cobj_ref privkey_biseg)
{
    ENGINE *e;
    const char *engine_id = "proc-engine";
    ENGINE_load_builtin_engines();
    e = ENGINE_by_id(engine_id);
    if(!e)
	throw basic_exception("Can't get engine %s", engine_id);    
    if(!ENGINE_init(e)) {
	/* the engine couldn't initialise, release 'e' */
	ENGINE_free(e);
	throw basic_exception("Couldn't init engine %s", engine_id);
    }
    
    if(!ENGINE_set_default_RSA(e))
	throw basic_exception("Couldn't sef default RSA engine %s", engine_id);

    debug_cprint(dbg, "loading rmt_privkey...");
    EVP_PKEY *pk = ENGINE_load_private_key(e, (char *)&privkey_biseg, 0, 0);
    debug_cprint(dbg, "loading rmt_privkey done!");

    if (!SSL_CTX_use_PrivateKey(ctx, pk))
	throw basic_exception("Couldn't use remote private key");
}

static void __attribute__((noreturn))
ssld_cow_entry(void)
{
    try {
	struct ssld_cow_args *d = (ssld_cow_args *) TLS_GATE_ARGS;

	if (!taint_cow(d->root_ct, COBJ(0, 0)))
	    throw error(-E_UNSPEC, "cow didn't happen?");

	tls_revalidate();
	thread_label_cache_invalidate();

	if (d->privkey_biseg.object)
	    ssl_load_rmt_privkey(the_ctx, d->privkey_biseg);

	stack_switch(d->cipher_biseg.container, d->cipher_biseg.object,
		     d->plain_biseg.container, d->plain_biseg.object,
		     cow_stacktop, (void *) &ssld_worker);
    
	cprintf("ssld_cow_entry: still running\n");
	thread_halt();
    } catch (std::exception &e) {
	cprintf("ssld_cow_entry: %s\n", e.what());
	thread_halt();
    }
}

static struct cobj_ref
ssld_cow_gate_create(uint64_t ct, uint64_t verify)
{
    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &ssld_cow_entry;
    te.te_stack = (char *) tls_stack_top - 8;
    error_check(sys_self_get_as(&te.te_as));

    label verify_label(2);
    verify_label.set(verify, 0);
    
    int64_t gate_id = sys_gate_create(ct, &te, 0, 0, 
				      verify_label.to_ulabel(), "ssld-cow", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    // COW'ed gate call copies mapping and segment
    uint64_t entry_ct = start_env->proc_container;

    struct cobj_ref stackobj;
    error_check(segment_alloc(entry_ct, PGSIZE, &stackobj,
			      0, 0, "gate thread stack"));
    scope_guard<int, cobj_ref> s(sys_obj_unref, stackobj);
    
    void *stackbase = 0;
    uint64_t stackbytes = thread_stack_pages * PGSIZE;
    error_check(segment_map(stackobj, 0, SEGMAP_READ | SEGMAP_WRITE |
			    SEGMAP_STACK | SEGMAP_REVERSE_PAGES,
			    &stackbase, &stackbytes, 0));
    char *stacktop = ((char *) stackbase) + stackbytes;
    
    s.dismiss();
    
    cow_stacktop = stacktop;
    
    return COBJ(ct, gate_id);
}

static SSL_CTX *
ssl_init(const char *server_pem, const char *dh_pem, const char *calist_pem)
{
    SSL_CTX *ctx;
    SSL_library_init();
    SSL_load_error_strings();

    SSL_METHOD *meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    // Load our keys and certificates
    if(!(SSL_CTX_use_certificate_chain_file(ctx, server_pem)))
	throw basic_exception("Can't read certificate file %s", server_pem);

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

    return ctx;
}

int
main (int ac, char **av)
{
    if (ac < 4) {
	cprintf("Usage: %s verify-handle server-pem dh-pem [servkey-pem]", 
		av[0]);
	return -1;
    }
    
    uint64_t verify;
    error_check(strtou64(av[1], 0, 10, &verify));
        
    const char *server_pem = av[2];
    const char *dh_pem = av[3];
    
    the_ctx = ssl_init(server_pem, dh_pem, 0);
    if (ac >= 5) {
	const char *servkey_pem = av[4];
	ssl_load_file_privkey(the_ctx, servkey_pem);

    }
    ssld_cow_gate_create(start_env->shared_container, verify);
        
    return 0;
}
