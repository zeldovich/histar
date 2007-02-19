#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djdebug.hh>

static void
msgcb(dj_delivery_code c, uint64_t token)
{
    warn << "sendcb: code " << c << ", token " << token << "\n";
}

static void
sndmsg(message_sender *s, dj_esign_pubkey node_pk, dj_message_endpoint ep)
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

    djs->set_delivery_cb(wrap(&dj_debug_delivery));
    sndmsg(djs, djs->pubkey(), ep);

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
