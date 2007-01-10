#include <fa.h>
#include <async.h>
#include <arpc.h>

fadd_arg arg;
fadd_res res;

static void 
getres(clnt_stat err) 
{
    if (err) 
	warn << "server: " << err << "\n";
    else if (res.error)
	warn << "error #" << res.error << "\n";
    else
	warn << "sum is " << *res.sum << "\n";
}

static void 
start(char *host, uint16_t port) 
{
    int fd = 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    uint32_t ip;
    if ((ip = inet_addr(host)) != INADDR_NONE)
	addr.sin_addr.s_addr = ip;
    else {  
	struct hostent *hostent;
	if ((hostent = gethostbyname(host)) == 0)
	    fatal << "unable to resolve " << host << "\n";
	memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length) ;
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	fatal << "socket error: " << fd << "\n";
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	close(fd);
	fatal << "connect error\n";
    }

    arg.var = "foo";
    arg.inc = 2;

    ref<axprt> x = axprt_stream::alloc(fd);
    ref<aclnt> c = aclnt::alloc (x, fadd_prog_1);
    c->call(FADDPROC_FADD, &arg, &res, wrap(getres));
}

int
main(int ac, char **av)
{
    char *host = av[1];
    int port = atoi(av[2]);
    start(host, port);
    amain();
    printf("done!\n");
    return 0;
}
