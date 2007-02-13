#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/djauth.h>
#include <dj/djgatecall.hh>

static void
logincb(dj_reply_status stat, const djcall_args *args)
{
    if (stat != REPLY_DONE) {
	warn << "logincb: stat " << stat << "\n";
	return;
    }

    djauth_reply reply;
    if (!str2xdr(reply, args->data)) {
	warn << "logincb: cannot decode\n";
	return;
    }

    if (!reply.ok) {
	warn << "logincb: not ok\n";
	return;
    }

    warn << "logincb: ok!\n";
}

static void
dostuff(djgate_caller *dc, str node_pk, dj_gatename gate, int taint_bit)
{
    djcall_args args;
    args.taint = label(1);
    args.grant = label(3);

#ifndef JOS_TEST
    if (taint_bit)
	args.taint.set(start_env->user_taint, 3);
#endif

    dj_reply_status stat;
    djcall_args res;

    djauth_request req;
    req.username = "root";
    req.password = "";
    args.data = xdr2str(req);

    stat = dc->call(node_pk, gate, args, &res);
    logincb(stat, &res);
}

int
main(int ac, char **av)
{
    if (ac != 4 && ac != 5) {
	printf("Usage: %s djd-gate-ct.id host-pk gate-ct.id [taint-bit]\n", av[0]);
	exit(-1);
    }

    cobj_ref djd_gate;
    djd_gate <<= av[1];
    const char *pk16 = av[2];

    dj_gatename gate;
    gate <<= av[3];

    dj_esign_pubkey k;
    k.n = bigint(pk16, 16);
    k.k = 8;
    str nodepk = xdr2str(k);

    int taint_bit = (ac == 4) ? 0 : atoi(av[4]);

    djgate_caller dc(djd_gate);
    dostuff(&dc, xdr2str(k), gate, taint_bit);
}
