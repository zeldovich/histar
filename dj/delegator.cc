#include <dj/delegator.hh>
#include <dj/internalx.h>
#include <dj/djops.hh>
#include <dj/djrpcx.h>

void
delegation_create(djprot *p, const dj_pubkey &sender,
		  const dj_message &m, const delivery_args &da)
{
    if (m.target.type != EP_DELEGATOR) {
	warn << "delegation_create: not a delegator target\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_call_msg callmsg;
    dj_delegate_arg darg;
    if (!bytes2xdr(callmsg, m.msg) || !bytes2xdr(darg, callmsg.buf)) {
	warn << "delegation_create: cannot unmarshal\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    if (callmsg.return_ep.type != EP_GATE && callmsg.return_ep.type != EP_SEGMENT) {
	warn << "delegation_create: must return to a gate or segment\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_delegate_res dres;
    for (uint32_t i = 0; i < darg.reqs.size(); i++) {
	const dj_delegate_req &dreq = darg.reqs[i];
	const dj_gcat &gcat = dreq.gcat;
	bool owner = false;
	for (uint32_t i = 0; i < m.glabel.ents.size(); i++)
	    if (m.glabel.ents[i] == gcat)
		owner = true;

	if (!owner) {
	    warn << "delegation_create: not owner\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	dj_stmt_signed ss;
	ss.stmt.set_type(STMT_DELEGATION);
	ss.stmt.delegation->a.set_type(ENT_PUBKEY);
	*ss.stmt.delegation->a.key = dreq.to;
	ss.stmt.delegation->b.set_type(ENT_GCAT);
	*ss.stmt.delegation->b.gcat = gcat;
	if (gcat.key != p->pubkey())
	    *ss.stmt.delegation->via.alloc() = p->pubkey();
	ss.stmt.delegation->from_ts = dreq.from_ts;
	ss.stmt.delegation->until_ts = dreq.until_ts;
	p->sign_statement(&ss);
	dres.delegations.push_back(ss);
    }

    dj_message replym;
    replym.target = callmsg.return_ep;
    replym.taint = m.taint;
    replym.catmap = callmsg.return_cm;
    replym.dset = callmsg.return_ds;
    replym.msg = xdr2str(dres);
    replym.want_ack = 0;

    p->send(sender, 0, m.dset, replym, 0, 0);
    da.cb(DELIVERY_DONE);
}
