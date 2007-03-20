#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
#error "Must use OpenSSL 0.9.6 or later"
#endif

// some knobs
static const int default_requests = 100;
static const int default_clients = 1;
static char *request_template_noauth = ""
    "GET %s HTTP/1.0\r\nUser-Agent: "
    "TestClient\r\nHost: %s:%d\r\n"
    "\r\n";
static char *request_template_auth = ""
    "GET %s HTTP/1.0\r\nUser-Agent: "
    "TestClient\r\nHost: %s:%d\r\n"
    "Authorization: Basic cm9vdDo=\r\n"
    "\r\n";
static char *request_template = 0;

static const char* path = "/";
static char logging = 0;
static char auth = 0;
static char warnings = 1;
static char host_hack = 0;
static const char session_reuse = 0;
static const int bufsize = 4096;

static const char *real_hosts[] = { "silas-jos64.scs.stanford.edu",
				    "alamo-square.scs.stanford.edu",
				    "class3.scs.stanford.edu",
				    "class4.scs.stanford.edu" };

static const char *vm_hosts[] = { "171.66.3.241",
				  "171.66.3.248",
				  "171.66.3.249" };

static const char **hosts = real_hosts;
static int num_hosts = 3;

static int 
err_exit(char *string)
{
    fprintf(stderr,"%d: %s\n", getpid(), string);
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
    
    request_len = strlen(request_template) + strlen(path) + strlen(host) + 6;
    if(!(request = (char *) malloc(request_len)))
	err_exit("Couldn't allocate request");
    snprintf(request, request_len, request_template, path, host, port);

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

    char first = 1;
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
	
	if (first) {
	    if (warnings && memcmp("HTTP/1.0 200", buf, strlen("HTTP/1.0 200")))
		fprintf(stderr, "HTTP error: %s\n", buf);
	    first = 0;
	}
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

uint32_t completed = 0;

static void
timeout(int signo)
{
    fprintf(stderr, "time limit up, completed %d!\n", completed);
    fprintf(stdout, "%d\n", completed);
    fflush(0);
    kill(0, SIGQUIT);
}

  
int 
main(int ac, char **av)
{
    int port;
    const char *host;
    uint32_t requests = default_requests;
    int clients = default_clients;
    int timelimit = 0;

    if (ac < 3) {
	fprintf(stderr, "Usage: %s host port "
		"[-r requests | -c clients | -l time-limit | "
		"-p path -a -d -s -h]\n", av[0]);
	exit(-1);
    }

    setpgrp();

    host = av[1];
    port = atoi(av[2]);

    int c;
    while ((c = getopt(ac, av, "r:c:l:p:dash:")) != -1) {
	switch(c) {
	case 'r':
	    requests = atoi(optarg);
	    break;
	case 'c':
	    clients = atoi(optarg);
	    break;
	case 'l':
	    timelimit = atoi(optarg);
	    break;
	case 'p':
	    path = strdup(optarg);
	    break;
	case 'd':
	    logging = 1;
	    break;
	case 'a':
	    auth = 1;
	    break;
	case 's':
	    warnings = 0;
	    break;
	case 'h':
	    num_hosts = atoi(optarg);
	    host_hack = 1;
	    break;
	}
    }

    if (auth)
	request_template = request_template_auth;
    else
	request_template = request_template_noauth;
    
    int fd[2];
    if (pipe(fd) < 0)
	err_exit("unable to create pipe");

    if (timelimit)
	requests = ~0;
    
    for (int i = 0; i < clients; i++) {
	pid_t p;
	if (p = fork()) {
	    fprintf(stderr, "%d (%d) started...\n", i, p);
	    continue;
	}

	int log = fd[1];
	close(fd[0]);

	if (host_hack)
	    host = hosts[i % num_hosts];

	struct hostent *hp;
	struct sockaddr_in addr;
	
	if(!(hp = gethostbyname(host)))
	    err_exit("Couldn't resolve host");
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr = *((struct in_addr*) hp->h_addr_list[0]);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	
	if (timelimit) {
	    // wait so all processes can be forked
	    kill(getpid(), SIGSTOP);
	}

	SSL_CTX *ctx = init_ctx();
	SSL_SESSION *ses = 0;
	
	if (session_reuse)
	    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);
	
	for (uint32_t i = 0; i < requests; i++) {
	    SSL *ssl;
	    BIO *sbio;
	    int sock;
	
	    sock = tcp_connect(&addr);
	    ssl = SSL_new(ctx);
	    sbio = BIO_new_socket(sock, BIO_NOCLOSE);
	    SSL_set_bio(ssl, sbio, sbio);
	    
	    if (session_reuse && ses)
		assert(SSL_set_session(ssl, ses));
	    
	    // init handshake w/ server
	    if(SSL_connect(ssl) <= 0)
		err_exit("SSL connect error");
	    
	    if (session_reuse && !SSL_session_reused(ssl))
		fprintf(stderr, "session not reused!\n");
	    
	    if (session_reuse && !ses)
		assert(ses = SSL_get_session(ssl));
	    
	    http_request(ssl, host, port);
	    if (timelimit)
		write(log, "*", 1);
	    SSL_free(ssl);
	    close(sock);
	}
    	destroy_ctx(ctx);
	return 0;
    }
    close(fd[1]);
    
    if (timelimit) {
	// make sure all processes have SIGSTOP
	usleep(1000000);
	fprintf(stderr, "starting clients...\n");
	
	signal(SIGALRM, &timeout);
	alarm(timelimit);
	kill(0, SIGCONT);

	// count connections for timelimit seconds
	for (;;) {
	    char a;
	    int r = read(fd[0], &a, 1);
	    if (r)
		completed++;
	}
    }

    for (int i = 0; i < clients; i++) {
	pid_t p;
	if ((p = wait(0)) < 0)
	    err_exit("wait error");
	fprintf(stderr, "%d (%d) terminated!\n", i, p);
    }
    return 0;
}
