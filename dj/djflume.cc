#include <dj/djrpcx.h>
#include <dj/djprotx.h>
#include <dj/internalx.h>
#include <dj/miscx.h>
#include <dj/djarpc.hh>
#include <dj/djflume.hh>
#include <dj/djlabel.hh>

extern "C" {
namespace flume {
#include "flume_cpp.h"
#include "flume_prot.h"
#include "flume_api.h"
#include "flume_clnt.h"
}
}

/*
 * XXX
 *
 * Right now nothing enforces the integrity of mappings.
 * We also don't have any mechanism for charging the user
 * for the mapping's resources, and the exporter's label
 * keeps growing.
 *
 * Should make mappings opaque in the protocol, so we can
 * add whatever we want to them (e.g. signature).
 */

void
flume_mapcreate::exec(const dj_message &m, const delivery_args &da)
{
    if (m.target.type != EP_MAPCREATE) {
	warn << "flume_mapcreate: not a mapcreate target\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_call_msg callmsg;
    dj_mapcreate_arg maparg;
    if (!bytes2xdr(callmsg, m.msg) || !bytes2xdr(maparg, callmsg.buf)) {
	warn << "flume_mapcreate: cannot unmarshal\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    if (callmsg.return_ep.type != EP_GATE &&
	callmsg.return_ep.type != EP_SEGMENT) {
	warn << "flume_mapcreate: must return to a gate or segment\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_mapcreate_res mapres;
    for (uint32_t i = 0; i < maparg.reqs.size(); i++) {
	const dj_mapreq &mapreq = maparg.reqs[i];

	dj_gcat gcat = mapreq.gcat;
	uint64_t lcat = mapreq.lcat;

	if (mapreq.lcat) {
	    warn << "flume_mapcreate: cannot create global categories\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	flume::x_handle_t h;
	int rc;
	if ((rc = flume::flume_new_handle(&h, 0, "mapped handle")) < 0) {
	    warn << "flume_mapcreate: cannot allocate a handle\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	dj_cat_mapping mapent;
	mapent.gcat = mapreq.gcat;
	mapent.lcat = h;
	mapent.res_ct = 0;
	mapent.res_gt = 0;
	mapres.mappings.push_back(mapent);
    }

    dj_message replym;
    replym.to = callmsg.return_host;
    replym.target = callmsg.return_ep;
    replym.taint = m.taint;
    replym.catmap = callmsg.return_cm;
    replym.dset = callmsg.return_ds;
    replym.msg = xdr2str(mapres);

    p_->send(replym, m.dset, 0, 0);
    da.cb(DELIVERY_DONE);
}

struct dj_flume_perl_state {
    dj_arpc_reply ar;
    char buf[4096];
    size_t bufcc;

    dj_flume_perl_state(const dj_arpc_reply &r) : ar(r), bufcc(0) {}
};

static void
dj_flume_perl_cb(int rev, dj_flume_perl_state *ps)
{
    if (ps->bufcc < sizeof(ps->buf)) {
	ssize_t cc = read(rev, &ps->buf[ps->bufcc], sizeof(ps->buf) - ps->bufcc);
	if (cc < 0) {
	    warn << "dj_flume_perl_cb: read error\n";
	} else if (cc > 0) {
	    ps->bufcc += cc;
	    return;
	}
    }

    str output(ps->buf, ps->bufcc);
    int errcode = 0;	/* should do waitpid really */

    perl_run_res pres;
    pres.retval = errcode;
    pres.output = output;

    ps->ar.r.msg.msg = xdr2str(pres);
    ps->ar.cb(true, ps->ar.r);

    close(rev);
    fdcb(rev, selread, 0);

    delete ps;
}

void
dj_flume_perl_svc(flume_mapcreate *fmc,
		  const dj_message &m, const str &s,
		  const dj_arpc_reply &ar)
{
    perl_run_arg parg;

    if (!str2xdr(parg, s)) {
	warn << "dj_flume_perl_svc: cannot decode\n";
	ar.cb(false, ar.r);
	return;
    }

    str input = parg.input;
    str script = parg.script;

    /*
     * For now, we only allow secrecy categories.
     * Trivial to extend to I & O.
     */
    flume::x_label_t *slabel = flume::label_alloc(m.taint.ents.size());

    /*
     * Use fmc to verify the mappings somehow.
     */
    dj_catmap_indexed cmap(m.catmap);
    for (uint32_t i = 0; i < m.taint.ents.size(); i++) {
	const dj_gcat &gcat = m.taint.ents[i];
	if (gcat.integrity) {
	    warn << "dj_flume_perl_svc: only secrecy categories for now\n";
	    ar.cb(false, ar.r);
	    label_free(slabel);
	    return;
	}

	uint64_t lcat;
	if (!cmap.g2l(gcat, &lcat, 0)) {
	    warn << "dj_flume_perl_svc: cannot map global cat\n";
	    ar.cb(false, ar.r);
	    label_free(slabel);
	    return;
	}

	label_set(slabel, i, lcat);
    }

    int forw, rev;
    flume::x_handlevec_t *fdh = flume::label_alloc(2);

    if (flume::flume_socketpair(flume::DUPLEX_ME_TO_THEM, &forw, &fdh->val[0], "fw") < 0 ||
	flume::flume_socketpair(flume::DUPLEX_THEM_TO_ME, &rev, &fdh->val[1], "rev") < 0)
    {
	warn << "dj_flume_perl_svc: flume_socketpair error\n";
	ar.cb(false, ar.r);
	return;
    }

    flume::x_labelset_t *ls = flume::labelset_alloc();
    flume::labelset_set_S(ls, slabel);

    if (flume::flume_set_fd_label(slabel, flume::LABEL_S, forw) < 0 ||
	flume::flume_set_fd_label(slabel, flume::LABEL_S, rev) < 0)
    {
	warn << "dj_flume_perl_svc: flume_set_fd_label error\n";
	ar.cb(false, ar.r);
	return;
    }

    const char *argv[4];
    argv[0] = "/disk/nickolai/flume/run/bin/flumeperl";
    argv[1] = "-e";
    argv[2] = script.cstr();
    argv[3] = 0;

    /* XXX flume_spawn has trouble passing arguments? */
    argv[1] = 0;

    flume::x_handle_t pid;
    extern char **environ;
    int rc = flume::flume_spawn(&pid, argv[0], (char *const *) argv,
				environ, 2, 0, ls, fdh, NULL);
    if (rc < 0) {
	warn << "dj_flume_perl_svc: flume_spawn error\n";
	ar.cb(false, ar.r);
	return;
    }

    label_free(fdh);
    label_free(slabel);
    labelset_free(ls);

    /* XXX flume_spawn has trouble passing arguments? */
    //write(forw, input.cstr(), input.len());
    write(forw, script.cstr(), script.len());
    close(forw);

    dj_flume_perl_state *ps = New dj_flume_perl_state(ar);
    fdcb(rev, selread, wrap(&dj_flume_perl_cb, rev, ps));
}

void
dj_flume_ctalloc_svc(flume_mapcreate *fmc,
		     const dj_message &m, const str &s,
		     const dj_arpc_reply &ar)
{
    dj_rpc_reply rr = ar.r;

    container_alloc_req carg;
    container_alloc_res cres;
    if (!str2xdr(carg, s)) {
	warn << "dj_flume_ctalloc_svc: cannot decode\n";
	ar.cb(false, rr);
	return;
    }

    cres.ct_id = 0xd0d0eeee;
    rr.msg.msg = xdr2str(cres);
    ar.cb(true, rr);
}
