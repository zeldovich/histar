#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>

static void
sendcb(dj_delivery_code c, uint64_t h)
{
    warn << "sendcb: code " << c << ", halted " << h << "\n";
}

static void
sndmsg(ptr<djprot> p, str node_pk, dj_message_endpoint endpt)
{
    warn << "sending a message..\n";

    dj_message_args a;
    a.send_timeout = 1;
    a.msg_ct = time(0);
    a.halted = 1234;
    a.namedcats.setsize(2);
    a.namedcats[0] = 123;
    a.namedcats[1] = 456;
    a.msg = "Hello world!";

    p->send(node_pk, endpt, a, wrap(&sendcb));
    delaycb(5, wrap(&sndmsg, p, node_pk, endpt));
}

int
main(int ac, char **av)
{
    dj_gatename gate;

    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    ptr<djprot> djs = djprot::alloc(port);

#ifdef JOS_TEST
    djs->set_catmgr(dj_dummy_catmgr());
    djs->set_delivery_cb(wrap(dj_debug_delivery));
#else
    ptr<catmgr> cmgr = dj_catmgr();
    //djs->set_callexec(wrap(dj_gate_exec, cmgr));
    djs->set_catmgr(cmgr);

    //ptr<djgate_incoming> incoming = dj_gate_incoming(djs);
    //cobj_ref ingate = incoming->gate();
    //warn << "djd incoming gate: " << ingate << "\n";
#endif

    dj_message_endpoint ep;
    ep.set_type(ENDPT_GATE);
    *ep.gate <<= "1.2";
    //sndmsg(djs, djs->pubkey(), ep);

    if (ac == 2) {
	str n(av[1]);
	dj_esign_pubkey k;
	k.n = bigint(n, 16);
	k.k = 8;

	//*ep.gate <<= av[2];

	sndmsg(djs, xdr2str(k), ep);
    }

    amain();
}
