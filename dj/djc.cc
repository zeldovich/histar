#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/gatesender.hh>
#include <dj/djsrpc.hh>

int
main(int ac, char **av)
{
    if (ac != 5) {
	printf("Usage: %s djd-gate-ct.id host-pk call-ct gate-ct.id\n", av[0]);
	exit(-1);
    }

    cobj_ref djd_gate;
    djd_gate <<= av[1];

    const char *pk16 = av[2];
    dj_pubkey k;
    k.n = bigint(pk16, 16);
    k.k = 8;

    uint64_t call_ct = atoi(av[3]);

    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    *ep.gate <<= av[4];

    gate_sender gs(djd_gate);

    dj_delegation_set dset;
    dj_catmap cm;
    dj_message m;

    m.target = ep;
    m.msg_ct = call_ct;
    m.token = 0;
    m.taint.deflevel = 1;
    m.glabel.deflevel = 3;
    m.gclear.deflevel = 0;

    dj_message replym;
    dj_delivery_code c = dj_rpc_call(&gs, k, 1, dset, cm, m,
				     "Hello world", &replym);
    warn << "code = " << c << "\n";
    if (c == DELIVERY_DONE)
	warn << replym;
}
