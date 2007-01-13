extern "C" {
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
}

#include <inc/error.hh>

static SSL_CTX *ctx;

static const char threaded = 1;
static const char *server_pem = "server.pem";
static const char *dh_pem = "dh.pem";

void profiler_init(void);

pthread_mutex_t mutex[128];

static void 
locking_function(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(&mutex[n]);
    else
        pthread_mutex_unlock(&mutex[n]);
}

static unsigned long 
id_function(void)
{
    return pthread_self();
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

static void*
http_client(void *arg)
{
    int s = (int64_t) arg;
    SSL *ssl;
    int r;

    try {
	error_check(ssld_accept(s, &ssl));
    } catch (std::exception &e) {
	printf("http_client: unable to accept ssl: %s\n", e.what());
	return 0;
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
	printf("http_client: connection error: %s\n", e.what());
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(s);
    return 0;
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

    if (threaded) {
	assert((sizeof(mutex) / sizeof(mutex[0])) > (uint32_t)CRYPTO_num_locks());
	CRYPTO_set_locking_callback(locking_function);
	CRYPTO_set_id_callback(id_function);
	for (int i = 0; i < sizeof(mutex) / sizeof(mutex[0]); i++)
	    pthread_mutex_init(&mutex[i], 0);
    }

    SSL_METHOD *meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    // Load our keys and certificates
    if(!(SSL_CTX_use_certificate_chain_file(ctx, server_pem)))
	throw basic_exception("Can't read certificate file %s\n", server_pem);
    
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
	if (SSL_CTX_set_tmp_dh(ctx, ret) < 0)
	    throw basic_exception("Couldn't set DH parameters using");
    }
}

int
main (int ac, char **av)
{
    ssl_init(server_pem, dh_pem, 0);
    
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        throw basic_exception("cannot create socket: %s", strerror(errno));
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(8080);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
	throw basic_exception("cannot bind socket: %s", strerror(errno));
    
    r = listen(s, 5);
    if (r < 0)
	throw basic_exception("cannot listen on socket: %s", strerror(r));
    
    printf("ssl_bench: server on port 8080\n");
    for (uint32_t i = 0; i < 100; i++) {
        socklen_t socklen = sizeof(sin);
	
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
	    printf("cannot accept client: %d\n", ss);
            continue;
        }
	
	if (threaded) {
	    pthread_t t;
	    r = pthread_create(&t, 0, http_client, (void *)(int64_t)ss);
	} else
	    http_client((void *) (int64_t)ss);
    }

    return 0;
}
