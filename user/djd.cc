#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/djfs.h>

static void
stuffcb(ptr<djprot> p, str node_pk, dj_reply_status stat, const djcall_args &args)
{
    warn << "stuffcb: status " << stat << "\n";
    if (stat == REPLY_DONE) {
	djfs_reply reply;
	if (!str2xdr(reply, args.data)) {
	    warn << "Stuffcb: cannot decode\n";
	} else {
	    if (reply.err) {
		warn << "stuffcb: err " << reply.err << "\n";
		return;
	    }

	    if (reply.d->op != DJFS_READDIR) {
		warn << "stuffcb: wrong reply op " << reply.d->op << "\n";
		return;
	    }

	    for (uint32_t i = 0; i < reply.d->ents->size(); i++)
		warn << "readdir: " << (*reply.d->ents)[i] << "\n";
	}
    }
}

static void
dostuff(ptr<djprot> p, str node_pk)
{
    djfs_request req;
    req.set_op(DJFS_READDIR);
    *req.pn = str("/bin");

    dj_gatename gate;
    djcall_args args;
    args.data = xdr2str(req);
    args.taint = label(1);
    args.grant = label(3);

    p->call(node_pk, gate, args, wrap(&stuffcb, p, node_pk));

    delaycb(5, wrap(&dostuff, p, node_pk));
}

int
main(int ac, char **av)
{
    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    ptr<djprot> djs = djprot::alloc(port);
    //djs->set_callexec(wrap(dj_dummy_exec));
    djs->set_callexec(wrap(dj_posixfs_exec));
    djs->set_catmgr(dj_dummy_catmgr());

    if (ac == 2) {
	str n(av[1]);
	dj_esign_pubkey k;
	k.n = bigint(n, 16);
	k.k = 8;
	dostuff(djs, xdr2str(k));
    }

    amain();
}
