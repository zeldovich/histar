extern "C" {
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/profiler.h>

#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>

#include <openssl/ssl.h>
}

#include <inc/error.hh>

static SSL_CTX *ctx;

static const char threaded = 1;
static const char *server_pem = "/bin/server.pem";
static const char *dh_pem = "/bin/dh.pem";
static jthread_mutex_t mutex[128];

static void 
locking_function(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
	jthread_mutex_lock(&mutex[n]);
    else
	jthread_mutex_unlock(&mutex[n]);
}

static unsigned long 
id_function(void)
{
    return sys_self_id();
}

static int
ssld_accept(int s, SSL **ret)
{
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
ssl_recv(SSL *ssl, void *buf, size_t count, int flags)
{
    int r = SSL_read(ssl, (char *)buf, count);
    if (r <= 0)
	return -1;
    return r;
}

static int
ssl_send(SSL *ssl, void *buf, size_t count, int flags)
{
    int r = SSL_write(ssl, (char *)buf, count);
    if (r <= 0)
	return -1;
    return r;
    
}

static void
http_client(void *arg)
{
    int s = (int64_t) arg;
    SSL *ssl;
    
    try {
	error_check(ssld_accept(s, &ssl));
    } catch (std::exception &e) {
	cprintf("http_client: unable to accept ssl: %s\n", e.what());
	close(s);
	return;
    }
    
    try {
	char buf[4096];
	error_check(ssl_recv(ssl, buf, sizeof(buf) - 1, 0));
	
	snprintf(buf, sizeof(buf),
		 "HTTP/1.0 200 OK\r\n"
		 "Content-Type: text/html\r\n"
		 "\r\n"
		 "<h1>Hello world.</h1>\r\n");
	
	error_check(ssl_send(ssl, buf, strlen(buf), 0));
    } catch (std::exception &e) {
	cprintf("http_client: connection error: %s\n", e.what());
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(s);
}

void
ssl_init(const char *server_pem,
	 const char *dh_pem, const char *calist_pem)
{
    if (ctx) {
	SSL_CTX_free(ctx);
	ctx = 0;
    }

    SSL_library_init();
    SSL_load_error_strings();

    assert((sizeof(mutex) / sizeof(mutex[0])) > (uint32_t)CRYPTO_num_locks());
    CRYPTO_set_locking_callback(locking_function);
    CRYPTO_set_id_callback(id_function);


    SSL_METHOD *meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    // Load our keys and certificates
    if(!(SSL_CTX_use_certificate_chain_file(ctx, server_pem)))
	throw basic_exception("Can't read certificate file %s", server_pem);
    
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
    ssl_init(server_pem, dh_pem, 0);
    
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
    
    printf("ssl_httpd: server on port 80\n");
    //profiler_init();
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            printf("cannot accept client: %d\n", ss);
            continue;
        }

	if (threaded) {
	    struct cobj_ref t;
	    r = thread_create(start_env->proc_container, &http_client,
			      (void*) (int64_t) ss, &t, "http client");
	    if (r < 0) {
		printf("cannot spawn client thread: %s\n", e2s(r));
		close(ss);
	    } else {
		fd_give_up_privilege(ss);
	    }
	} else {
	    http_client((void *) (int64_t)ss);
	}
    }

    return 0;
}
