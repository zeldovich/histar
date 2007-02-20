#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djdebug.hh>
#include <dj/directexec.hh>
#include <dj/djrpc.hh>
#include <dj/djfs.h>
#include <dj/djfs_posix.hh>

static void
fsrpccb(ptr<dj_rpc_call>, dj_delivery_code c, const dj_message *m)
{
    warn << "fsrpccb: code " << c << "\n";
    if (c == DELIVERY_DONE) {
	djfs_reply rep;
	if (!str2xdr(rep, str(m->msg.base(), m->msg.size()))) {
	    warn << "fsrpccb: cannot unmarshal\n";
	    return;
	}

	if (rep.err) {
	    warn << "fsrpccb: err " << rep.err << "\n";
	    return;
	}

	if (rep.d->op == DJFS_READDIR) {
	    for (uint32_t i = 0; i < rep.d->readdir->ents.size(); i++)
		warn << "readdir: " << rep.d->readdir->ents[i] << "\n";
	} else if (rep.d->op == DJFS_READ) {
	    str d(rep.d->read->data.base(), rep.d->read->data.size());
	    warn << "read: " << d << "\n";
	} else {
	    warn << "strange reply op " << rep.d->op << "\n";
	}
    }
}

static void
sndfsrpc(message_sender *s, dj_gate_factory *f, dj_pubkey node_pk, dj_message_endpoint ep)
{
    dj_message m;
    m.target = ep;
    m.msg_ct = 12345;
    m.taint.deflevel = 1;
    m.glabel.deflevel = 3;
    m.gclear.deflevel = 0;

    djfs_request req;
    req.set_op(DJFS_READDIR);
    req.readdir->pn = "/bin";

    dj_delegation_set dset;
    ptr<dj_rpc_call> rc = New refcounted<dj_rpc_call>(s, f, 9876);
    rc->call(node_pk, 1, dset, m, xdr2str(req), wrap(&fsrpccb, rc));
    delaycb(5, wrap(&sndfsrpc, s, f, node_pk, ep));
}

static void
rpccb(ptr<dj_rpc_call>, dj_delivery_code c, const dj_message *m)
{
    warn << "rpccb: code " << c << "\n";
    if (c == DELIVERY_DONE)
	warn << *m;
}

static void
sndrpc(message_sender *s, dj_gate_factory *f, dj_pubkey node_pk, dj_message_endpoint ep)
{
    dj_message m;
    m.target = ep;
    m.msg_ct = 12345;
    m.taint.deflevel = 1;
    m.glabel.deflevel = 3;
    m.gclear.deflevel = 0;

    dj_delegation_set dset;
    ptr<dj_rpc_call> rc = New refcounted<dj_rpc_call>(s, f, 9876);
    rc->call(node_pk, 1, dset, m, "Hello world.", wrap(&rpccb, rc));
    delaycb(5, wrap(&sndrpc, s, f, node_pk, ep));
}

static void
msgcb(dj_delivery_code c, uint64_t token)
{
    warn << "sendcb: code " << c << ", token " << token << "\n";
}

static void
sndmsg(message_sender *s, dj_pubkey node_pk, dj_message_endpoint ep)
{
    warn << "sending a message..\n";

    dj_message a;
    a.target = ep;
    a.msg_ct = 5432;
    a.token = 1234;
    a.taint.deflevel = 1;
    a.glabel.deflevel = 3;
    a.gclear.deflevel = 0;
    a.msg = "Hello world!";

    dj_delegation_set dset;
    s->send(node_pk, 1, dset, a, wrap(&msgcb));
    delaycb(5, wrap(&sndmsg, s, node_pk, ep));
}

int
main(int ac, char **av)
{
    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    *ep.gate <<= "5.7";

    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    djprot *djs = djprot::alloc(port);

    dj_direct_gatemap gm;
    djs->set_delivery_cb(wrap(&gm, &dj_direct_gatemap::deliver));

    ep = gm.create_gate(1, wrap(&dj_debug_sink));
    warn << "dj_debug_sink on " << ep << "\n";
    //sndmsg(djs, djs->pubkey(), ep);

    ep = gm.create_gate(1, wrap(&dj_rpc_srv_sink, djs, wrap(&dj_echo_service)));
    warn << "dj_echo_service on " << ep << "\n";
    //sndrpc(djs, &gm, djs->pubkey(), ep);

    ep = gm.create_gate(1, wrap(&dj_rpc_srv_sink, djs, wrap(&dj_posixfs_service)));
    warn << "dj_posixfs_service on " << ep << "\n";
    //sndfsrpc(djs, &gm, djs->pubkey(), ep);

    if (ac == 3) {
	str n(av[1]);
	dj_pubkey k;
	k.n = bigint(n, 16);
	k.k = 8;

	*ep.gate <<= av[2];

	sndfsrpc(djs, &gm, k, ep);
    }

    amain();
}
