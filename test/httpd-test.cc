#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
#error "Must use OpenSSL 0.9.6 or later"
#endif

// some knobs
static const int default_requests = 100;
static char *request_template = 
    "GET / HTTP/1.0\r\nUser-Agent:"
    "TestClient\r\nHost: %s:%d\r\n\r\n";
static const char logging = 0;
static const int bufsize = 4096;

static int 
err_exit(char *string)
{
    fprintf(stderr,"%s\n",string);
    exit(-1);
}

SSL_CTX *
init_ctx(void)
{
    SSL_METHOD *meth;
    SSL_CTX *ctx;
    
    SSL_library_init();
    
    // nice error messages
    SSL_load_error_strings();
    
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    return ctx;
}
     
void 
destroy_ctx(SSL_CTX *ctx)
{
    SSL_CTX_free(ctx);
}

static int 
tcp_connect(struct sockaddr_in *addr)
{
    int sock;

    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
      err_exit("Couldn't create socket");
    if(connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
	err_exit("Couldn't connect socket");
    
    return sock;
}

static int 
http_request(SSL *ssl, const char *host, int port)
{
    char *request=0;
    char buf[bufsize];
    int r;
    int len, request_len;
    
    request_len = strlen(request_template) + strlen(host) + 6;
    if(!(request = (char *) malloc(request_len)))
	err_exit("Couldn't allocate request");
    snprintf(request, request_len, request_template, host, port);

    request_len = strlen(request);
    r = SSL_write(ssl, request, request_len);

    switch(SSL_get_error(ssl, r)) {      
    case SSL_ERROR_NONE:
        if(request_len != r)
	    err_exit("Incomplete write!");
        break;
    default:
	err_exit("SSL write problem");
    }
    
    if (logging)
	printf("--server response start--\n");
    
    while (1) {
	r = SSL_read(ssl, buf, sizeof(buf));
	switch(SSL_get_error(ssl, r)){
        case SSL_ERROR_NONE:
	    len = r;
	    break;
        case SSL_ERROR_ZERO_RETURN:
	    goto shutdown;
        default:
	    err_exit("SSL read problem");
	}
	if (logging)
	    fwrite(buf, 1, len, stdout);
    }
       
 shutdown:
    if (logging)
	printf("--server response end--\n");
    
    r = SSL_shutdown(ssl);
    switch(r){
    case 1:
        break;
    case 0:
    case -1:
    default:
        err_exit("Shutdown failed");
    }
    
    free(request);
    return 0;
}
    
int 
main(int ac, char **av)
{
    int port;
    const char *host;
    int requests = default_requests;

    if (ac < 3) {
	fprintf(stderr, "Usage: %s host port [requests]\n", av[0]);
	exit(-1);
    }

    host = av[1];
    port = atoi(av[2]);
    if (ac >= 4)
	requests = atoi(av[3]);
    
    struct hostent *hp;
    struct sockaddr_in addr;
    
    if(!(hp = gethostbyname(host)))
	err_exit("Couldn't resolve host");
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr = *((struct in_addr*) hp->h_addr_list[0]);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    SSL_CTX *ctx = init_ctx();
    
    for (int i = 0; i < requests; i++) {
	SSL *ssl;
	BIO *sbio;
	int sock;

	sock = tcp_connect(&addr);
    	ssl = SSL_new(ctx);
	sbio = BIO_new_socket(sock, BIO_NOCLOSE);
	SSL_set_bio(ssl, sbio, sbio);
        // init handshake w/ server
	if(SSL_connect(ssl) <= 0)
	    err_exit("SSL connect error");
 
	http_request(ssl, host, port);
	SSL_free(ssl);
        close(sock);
    }
    
    destroy_ctx(ctx);

    return 0;
}
