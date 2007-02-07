#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/djfs.h>
#include <dj/directexec.hh>

static void
readcb(dj_reply_status stat, const djcall_args *args)
{
    if (stat != REPLY_DONE) {
	warn << "readcb: stat " << stat << "\n";
	return;
    }

    djfs_reply reply;
    if (!str2xdr(reply, args->data)) {
	warn << "readcb: cannot decode\n";
	return;
    }

    if (reply.d->op != DJFS_READ) {
	warn << "readcb: wrong reply op\n";
	return;
    }

    str d(reply.d->read->data.base(), reply.d->read->data.size());
    warn << "read: " << d << "\n";
}

static void
readdircb(ptr<djprot> p, str node_pk, dj_reply_status stat, const djcall_args *args)
{
    if (stat == REPLY_DONE) {
	djfs_reply reply;
	if (!str2xdr(reply, args->data)) {
	    warn << "readdircb: cannot decode\n";
	} else {
	    if (reply.err) {
		warn << "readdircb: err " << reply.err << "\n";
		return;
	    }

	    if (reply.d->op != DJFS_READDIR) {
		warn << "readdircb: wrong reply op " << reply.d->op << "\n";
		return;
	    }

	    for (uint32_t i = 0; i < reply.d->readdir->ents.size(); i++)
		warn << "readdir: " << reply.d->readdir->ents[i] << "\n";
	}
    } else {
	warn << "readdircb: status " << stat << "\n";
    }
}

static void
dostuff(ptr<djprot> p, str node_pk)
{
    djfs_request req;
    req.set_op(DJFS_READDIR);
    req.readdir->pn = str("/bin");

    dj_gatename gate;
    gate.gate_ct = 1;
    gate.gate_id = 2;
    djcall_args args;
    args.data = xdr2str(req);
    args.taint = label(1);
    args.grant = label(3);

    p->call(node_pk, gate, args, wrap(&readdircb, p, node_pk));

    req.set_op(DJFS_READ);
    req.read->pn = str("/etc/passwd");
    args.data = xdr2str(req);
    p->call(node_pk, gate, args, wrap(&readcb));

    delaycb(5, wrap(&dostuff, p, node_pk));
}

int
main(int ac, char **av)
{
    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    ptr<djprot> djs = djprot::alloc(port);

#ifdef JOS_TEST
    dj_direct_gatemap gm;
    djs->set_callexec(wrap(&gm, &dj_direct_gatemap::newexec));
    djs->set_catmgr(dj_dummy_catmgr());

    gm.gatemap_.insert(COBJ(1, 2), wrap(dj_posixfs_service));
    gm.gatemap_.insert(COBJ(1, 3), wrap(dj_echo_service));
#else
    djs->set_callexec(wrap(dj_gate_exec));
    djs->set_catmgr(dj_dummy_catmgr());
#endif

    if (ac == 2) {
	str n(av[1]);
	dj_esign_pubkey k;
	k.n = bigint(n, 16);
	k.k = 8;
	dostuff(djs, xdr2str(k));
    }

    amain();
}
