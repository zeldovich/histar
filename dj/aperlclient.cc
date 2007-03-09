#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djarpc.hh>
#include <dj/internalx.h>
#include <dj/miscx.h>
#include <dj/execmux.hh>
#include <dj/stuff.hh>
#include <dj/directexec.hh>
#include <dj/delegator.hh>

struct perl_req {
    djprot *p;
    dj_gate_factory *f;
    dj_pubkey k;
    uint64_t call_ct;
    uint64_t taint_ct;
    dj_gatename srvgate;
    dj_gcat gcat;

    dj_delegation_set dset;
    dj_catmap catmap;
};

static void
perl_cb(perl_req *pr, ptr<dj_arpc_call> old_call,
	dj_delivery_code c, const dj_message *rm)
{
    warn << "perl_cb: code " << c << "\n";
    if (c == DELIVERY_DONE) {
	perl_run_res pres;
	assert(bytes2xdr(pres, rm->msg));

	warn << "Perl exit code: " << pres.retval << "\n";
	warn << "Perl output: " << pres.output << "\n";
    }
}

static void
ctalloc_cb(perl_req *pr, ptr<dj_arpc_call> old_call,
	   dj_delivery_code c, const dj_message *rm)
{
    container_alloc_res ctres;
    if (c != DELIVERY_DONE)
	fatal << "ctalloc_cb: code " << c << "\n";
    assert(bytes2xdr(ctres, rm->msg));
    pr->taint_ct = ctres.ct_id;

    /* Send a real request now.. */
    perl_run_arg parg;
    parg.script = str("print 'A'x5; print <>;");
    parg.input = str("Hello world.");

    dj_message m;
    m.target.set_type(EP_GATE);
    m.target.ep_gate->msg_ct = pr->taint_ct;
    m.target.ep_gate->gate = pr->srvgate;
    m.dset = pr->dset;
    m.catmap = pr->catmap;
    m.taint.ents.push_back(pr->gcat);

    ptr<dj_arpc_call> call = New refcounted<dj_arpc_call>(pr->p, pr->f, 0xdead);
    call->call(pr->k, 1, pr->dset, m, xdr2str(parg),
	       wrap(&perl_cb, pr, call), &pr->catmap, &pr->dset);
}

static void
delegate_cb(perl_req *pr, ptr<dj_arpc_call> old_call,
	    dj_delivery_code c, const dj_message *rm)
{
    dj_stmt_signed ss;
    assert(c == DELIVERY_DONE);
    assert(bytes2xdr(ss, rm->msg));

    rpc_bytes<2147483647ul> s;
    xdr2bytes(s, ss);
    pr->dset.ents.push_back(s);

    /* Create a remote tainted container */
    container_alloc_req ctreq;
    ctreq.parent = pr->call_ct;
    ctreq.quota = CT_QUOTA_INF;
    ctreq.timeout_msec = 5000;
    ctreq.label.ents.push_back(pr->gcat);

    dj_message m;
    m.target.set_type(EP_GATE);
    m.target.ep_gate->msg_ct = pr->call_ct;
    m.target.ep_gate->gate.gate_ct = 0;
    m.target.ep_gate->gate.gate_id = GSPEC_CTALLOC;
    m.dset = pr->dset;
    m.catmap = pr->catmap;
    m.glabel.ents.push_back(pr->gcat);
    m.gclear.ents.push_back(pr->gcat);

    ptr<dj_arpc_call> call = New refcounted<dj_arpc_call>(pr->p, pr->f, 0xdead);
    call->call(pr->k, 1, pr->dset, m, xdr2str(ctreq),
	       wrap(&ctalloc_cb, pr, call), &pr->catmap, &pr->dset);
}

static void
map_create_cb(perl_req *pr, ptr<dj_arpc_call> old_call,
	      dj_delivery_code c, const dj_message *rm)
{
    dj_cat_mapping cme;
    assert(c == DELIVERY_DONE);
    assert(bytes2xdr(cme, rm->msg));
    pr->catmap.ents.push_back(cme);

    /* Create a delegation for the remote host */
    dj_delegate_req dreq;
    dreq.gcat = pr->gcat;
    dreq.to = pr->k;
    dreq.from_ts = 0;
    dreq.until_ts = ~0;

    dj_message m;
    m.target.set_type(EP_DELEGATOR);
    m.glabel.ents.push_back(pr->gcat);

    ptr<dj_arpc_call> call = New refcounted<dj_arpc_call>(pr->p, pr->f, 0xdead);
    call->call(pr->p->pubkey(), 1, pr->dset, m, xdr2str(dreq),
	       wrap(&delegate_cb, pr, call));
}

static void
do_stuff(perl_req *pr)
{
    /* Fabricate a new global category for tainting */
    pr->gcat.key = pr->p->pubkey();
    pr->gcat.id = 0xc0ffee;
    pr->gcat.integrity = 0;

    /* Create a mapping on the remote machine */
    dj_mapreq mapreq;
    mapreq.ct = pr->call_ct;
    mapreq.gcat = pr->gcat;
    mapreq.lcat = 0;

    dj_message m;
    m.target.set_type(EP_MAPCREATE);

    ptr<dj_arpc_call> call = New refcounted<dj_arpc_call>(pr->p, pr->f, 0xdead);
    call->call(pr->k, 1, pr->dset, m, xdr2str(mapreq),
	       wrap(&map_create_cb, pr, call));
}

int
main(int ac, char **av)
{
    if (ac != 4) {
	printf("Usage: %s host-pk call-ct perl-gate\n", av[0]);
	exit(-1);
    }

    ptr<sfspub> sfspub = sfscrypt.alloc(av[1], SFS_VERIFY | SFS_ENCRYPT);
    assert(sfspub);

    perl_req *pr = New perl_req();
    pr->k = sfspub2dj(sfspub);
    pr->call_ct = atoi(av[2]);
    pr->srvgate <<= av[3];

    uint16_t port = 5923;
    djprot *djs = djprot::alloc(port);

    exec_mux emux;
    djs->set_delivery_cb(wrap(&emux, &exec_mux::exec));

    dj_direct_gatemap gm;
    emux.set(EP_GATE, wrap(&gm, &dj_direct_gatemap::deliver));
    emux.set(EP_DELEGATOR, wrap(&delegation_create, djs));

    pr->p = djs;
    pr->f = &gm;

    delaycb(5, wrap(&do_stuff, pr));
    amain();
}
