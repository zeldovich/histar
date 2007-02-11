#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/djfs.h>
#include <dj/djgatecall.hh>

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
readdircb(dj_reply_status stat, const djcall_args *args)
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
dostuff(djgate_caller *dc, str node_pk, uint64_t ct, uint64_t id)
{
    djfs_request req;

    dj_gatename gate;
    gate.gate_ct = ct;
    gate.gate_id = id;

    djcall_args args;
    args.taint = label(1);
    args.grant = label(3);

    dj_reply_status stat;
    djcall_args res;

    for (;;) {
	req.set_op(DJFS_READDIR);
	req.readdir->pn = str("/bin");
	args.data = xdr2str(req);

	stat = dc->call(node_pk, gate, args, &res);
	readdircb(stat, &res);

	req.set_op(DJFS_READ);
	req.read->pn = str("/etc/passwd");
	args.data = xdr2str(req);

	stat = dc->call(node_pk, gate, args, &res);
	readcb(stat, &res);

	sleep(5);
    }
}

int
main(int ac, char **av)
{
    if (ac != 5) {
	printf("Usage: %s djd-gate-ct djd-gate-id host-pk gate-ct gate-id\n", av[0]);
	exit(-1);
    }

    cobj_ref djd_gate;
    djd_gate.container = atoi(av[1]);
    djd_gate.object = atoi(av[2]);
    const char *pk16 = av[3];

    uint64_t gct = atoi(av[4]);
    uint64_t gid = atoi(av[5]);

    dj_esign_pubkey k;
    k.n = bigint(pk16, 16);
    k.k = 8;
    str nodepk = xdr2str(k);

    djgate_caller dc(djd_gate);
    dostuff(&dc, xdr2str(k), gct, gid);
}
