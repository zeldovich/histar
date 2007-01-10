#include <fa.h>
#include <async.h>
#include <arpc.h>
#include <str.h>
#include <qhash.h>

static qhash<str, int> table;

static void 
dofadd(fadd_arg *arg, fadd_res *res) 
{
    int *valp = table[arg->var];
    if (valp) {
	res->set_error(0);
	*res->sum = *valp += arg->inc;
    } else
	res->set_error(1);
}

static ptr<asrv> s;

static void
dispatch(svccb *sbp)
{
    if (!sbp) {
	s = NULL;
	return;
    }
    
    switch(sbp->proc()) {
    case FADDPROC_NULL:
	sbp->reply(NULL);
	break;
    case FADDPROC_FADD: {
	fadd_res res;
	dofadd(sbp->getarg<fadd_arg> (), &res);
	sbp->reply(&res);
	break;
    }
    default:
	sbp->reject(PROC_UNAVAIL);
    }
}

static void 
getnewclient(int fd)
{
    s = asrv::alloc(axprt_stream::alloc(fd), fadd_prog_1, wrap(dispatch));
}


int
main(int ac, char **av)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(22);
    int r = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
	fatal << "error binding socket\n";
    
    r = listen(fd, 5); 
    if (r < 0)
	fatal << "error listening socket\n";

    socklen_t socklen = sizeof(sin);

    table.insert("foo", 1);

    warn << "waiting for connection...\n";
    
    int ss = accept(fd, (struct sockaddr *)&sin, &socklen);
    if (ss < 0)
	fatal << "accept failed\n";

    make_async(ss);
    getnewclient(ss);

    warn << "connection init, calling amain\n";

    amain();
    return 0;
}
