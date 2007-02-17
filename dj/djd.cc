#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/directexec.hh>

static void
sendcb(dj_delivery_code c, uint64_t token)
{
    warn << "sendcb: code " << c << ", token " << token << "\n";
}

static void
sndmsg(message_sender *s, dj_esign_pubkey node_pk, dj_message_endpoint endpt)
{
    warn << "sending a message..\n";

    dj_message_args a;
    a.send_timeout = 1;
    a.msg_ct = time(0);
    a.token = 1234;
    a.namedcats.setsize(2);
    a.namedcats[0] = 123;
    a.namedcats[1] = 456;
    a.msg = "Hello world!";

    s->send(node_pk, endpt, a, wrap(&sendcb));
    delaycb(5, wrap(&sndmsg, s, node_pk, endpt));
}

int
main(int ac, char **av)
{
    dj_message_endpoint ep;

    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    djprot *djs = djprot::alloc(port);

#ifdef JOS_TEST
    dj_direct_gatemap gm;

    //ep = gm.create_gate(1, wrap(&dj_echo_sink));
    ep = gm.create_gate(1, wrap(&dj_debug_sink));
    warn << "echo_sink on endpoint " << ep << "\n";

    djs->set_catmgr(dj_dummy_catmgr());
    djs->set_delivery_cb(wrap(&gm, &dj_direct_gatemap::deliver));
    sndmsg(djs, djs->pubkey(), ep);
#else
    ptr<catmgr> cmgr = dj_catmgr();
    //djs->set_callexec(wrap(dj_gate_exec, cmgr));
    djs->set_catmgr(cmgr);

    //ptr<djgate_incoming> incoming = dj_gate_incoming(djs);
    //cobj_ref ingate = incoming->gate();
    //warn << "djd incoming gate: " << ingate << "\n";
#endif

    if (ac == 3) {
	str n(av[1]);
	dj_esign_pubkey k;
	k.n = bigint(n, 16);
	k.k = 8;

	*ep.gate <<= av[2];

	sndmsg(djs, k, ep);
    }

    amain();
}
