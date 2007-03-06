#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djdebug.hh>
#include <dj/directexec.hh>
#include <dj/djarpc.hh>
#include <dj/djfs.h>
#include <dj/djfs_posix.hh>
#include <dj/catmgr.hh>
#include <dj/gateincoming.hh>
#include <dj/gateexec.hh>
#include <dj/execmux.hh>
#include <dj/mapcreate.hh>
#include <dj/delegator.hh>
#include <dj/hsutil.hh>
#include <dj/djkey.hh>

static void
fsrpccb(ptr<dj_arpc_call>, dj_delivery_code c, const dj_message *m)
{
    warn << "fsrpccb: code " << c << "\n";
    if (c == DELIVERY_DONE) {
	djfs_reply rep;
	if (!bytes2xdr(rep, m->msg)) {
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

    djfs_request req;
    req.set_op(DJFS_READDIR);
    req.readdir->pn = "/bin";

    dj_delegation_set dset;
    ptr<dj_arpc_call> rc = New refcounted<dj_arpc_call>(s, f, 9876);
    rc->call(node_pk, 1, dset, m, xdr2str(req), wrap(&fsrpccb, rc));
    delaycb(5, wrap(&sndfsrpc, s, f, node_pk, ep));
}

static void
rpccb(ptr<dj_arpc_call>, dj_delivery_code c, const dj_message *m)
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

    dj_delegation_set dset;
    ptr<dj_arpc_call> rc = New refcounted<dj_arpc_call>(s, f, 9876);
    rc->call(node_pk, 1, dset, m, "Hello world.", wrap(&rpccb, rc));
    delaycb(5, wrap(&sndrpc, s, f, node_pk, ep));
}

static void
msgcb(dj_delivery_code c)
{
    warn << "sendcb: code " << c << "\n";
}

static void
sndmsg(message_sender *s, dj_pubkey node_pk, dj_message_endpoint ep)
{
    warn << "sending a message..\n";

    dj_message a;
    a.target = ep;
    a.msg = "Hello world!";

    dj_delegation_set dset;
    s->send(node_pk, 1, dset, a, wrap(&msgcb), 0);
    delaycb(5, wrap(&sndmsg, s, node_pk, ep));
}

#ifdef JOS_TEST
static uint64_t
time_usec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    uint64_t usec = tv.tv_sec;
    usec = usec * 1000000 + tv.tv_usec;
    return usec;
}
#endif

int
main(int ac, char **av)
{
    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = 12345;
    ep.ep_gate->gate <<= "5.7";

    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    djprot *djs = djprot::alloc(port);

    exec_mux emux;
    djs->set_delivery_cb(wrap(&emux, &exec_mux::exec));
    emux.set(EP_DELEGATOR, wrap(&delegation_create, djs));

#ifdef JOS_TEST
    dj_direct_gatemap gm;
    emux.set(EP_GATE, wrap(&gm, &dj_direct_gatemap::deliver));

    ep = gm.create_gate(1, wrap(&dj_debug_sink));
    warn << "dj_debug_sink on " << ep << "\n";
    //sndmsg(djs, djs->pubkey(), ep);

    ep = gm.create_gate(1, wrap(&dj_arpc_srv_sink, djs, wrap(&dj_rpc_to_arpc, wrap(&dj_echo_service))));
    warn << "dj_echo_service on " << ep << "\n";
    //sndrpc(djs, &gm, djs->pubkey(), ep);

    ep = gm.create_gate(1, wrap(&dj_arpc_srv_sink, djs, wrap(&dj_rpc_to_arpc, wrap(&dj_posixfs_service))));
    warn << "dj_posixfs_service on " << ep << "\n";
    //sndfsrpc(djs, &gm, djs->pubkey(), ep);

    dj_stmt_signed ss;
    ss.stmt.set_type(STMT_DELEGATION);
    ss.stmt.delegation->a.set_type(ENT_PUBKEY);
    ss.stmt.delegation->b.set_type(ENT_PUBKEY);
    *ss.stmt.delegation->a.key = djs->pubkey();
    *ss.stmt.delegation->b.key = djs->pubkey();

    uint64_t start, end;
    uint64_t count = 100;

    start = time_usec();
    for (uint32_t i = 0; i < count; i++)
	djs->sign_statement(&ss);
    end = time_usec();
    printf("Signature: %ld usec\n", (end - start) / count);

    start = time_usec();
    for (uint32_t i = 0; i < count; i++)
	assert(verify_stmt(ss));
    end = time_usec();
    printf("Verify: %ld usec\n", (end - start) / count);

    if (ac == 3) {
	str pubstr(av[1]);
	ptr<sfspub> sfspub = sfscrypt.alloc(pubstr, SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	dj_pubkey k = sfspub2dj(sfspub);

	ep.ep_gate->gate <<= av[2];

	sndfsrpc(djs, &gm, k, ep);
    }
#endif

#ifndef JOS_TEST
    catmgr *cm = catmgr::alloc();
    dj_incoming_gate *in = dj_incoming_gate::alloc(djs, cm, start_env->shared_container);
    warn << "dj_incoming_gate at " << in->gate() << "\n";

    emux.set(EP_GATE, wrap(&gate_exec, cm, in->gate()));
    warn << "delivering gate messages via gate_exec\n";

    histar_mapcreate hmc(djs, cm);
    emux.set(EP_MAPCREATE, wrap(&hmc, &histar_mapcreate::exec));

    str_to_segment(start_env->shared_container,
		   xdr2str(djs->pubkey()), "selfkey");
#endif

    amain();
}
