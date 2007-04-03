#include <inc/labelutil.hh>
#include <inc/privstore.hh>
#include <inc/scopeguard.hh>
#include <dj/mapcreate.hh>
#include <dj/internalx.h>
#include <dj/djlabel.hh>
#include <dj/djops.hh>
#include <dj/djrpcx.h>

void
histar_mapcreate::exec(const dj_message &m, const delivery_args &da)
{
    if (m.target.type != EP_MAPCREATE) {
	warn << "histar_mapcreate: not a mapcreate target\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    local_mapcreate_state *lms = (local_mapcreate_state *) da.local_delivery_arg;

    label vl, vc;
    dj_label reply_taint;

    dj_catmap_indexed cmi(m.catmap);
    try {
	if (lms) {
	    vl = *lms->vl;
	    vc = *lms->vc;

	    label taint = vl;
	    taint.transform(label::star_to, taint.get_default());
	    label_to_djlabel(cmi, taint, &reply_taint, label_taint);
	} else {
	    label t, g;

	    djlabel_to_label(cmi, m.taint,  &t, label_taint);
	    djlabel_to_label(cmi, m.glabel, &g, label_owner, true);
	    t.merge(&g, &vl, label::min, label::leq_starlo);
	    vc = t;
	    reply_taint = m.taint;
	}
    } catch (std::exception &e) {
	warn << "histar_mapcreate(1): " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    verify_label_reqctx ctx(vl, vc);
    try {
	cm_->acquire(m.catmap, true);
    } catch (std::exception &e) {
	warn << "histar_mapcreate(2): " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    dj_call_msg callmsg;
    dj_mapcreate_arg maparg;
    if (!bytes2xdr(callmsg, m.msg) || !bytes2xdr(maparg, callmsg.buf)) {
	warn << "histar_mapcreate: cannot unmarshal\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    if (callmsg.return_ep.type != EP_GATE && callmsg.return_ep.type != EP_SEGMENT) {
	warn << "histar_mapcreate: must return to a gate or segment\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_mapcreate_res mapres;
    for (uint32_t i = 0; i < maparg.reqs.size(); i++) {
	const dj_mapreq &mapreq = maparg.reqs[i];

	dj_gcat gcat = mapreq.gcat;
	uint64_t lcat = mapreq.lcat;

	if (lcat) {		/* Create a global category */
	    if (cmi.l2g(lcat, &gcat)) {
		// Caller already provided an existing mapping for lcat,
		// so just create a new mapping.
	    } else {
		gcat.key = p_->pubkey();
		gcat.id = ++counter_;

		// Caller better have granted us the star on gate invocation,
		// because this is the first mapping for this category.
		if (!lms) {
		    warn << "histar_mapcreate: missing local_delivery_arg\n";
		    da.cb(DELIVERY_REMOTE_ERR);
		    return;
		}

		label gl;
		obj_get_label(lms->privgate, &gl);
		if (gl.get(lcat) != LB_LEVEL_STAR) {
		    warn << "histar_mapcreate: trying to map unknown lcat\n";
		    da.cb(DELIVERY_REMOTE_ERR);
		    return;
		}

		saved_privilege sp(lcat, lms->privgate);
		sp.acquire();
	    }
	} else {
	    if (cmi.g2l(gcat, &lcat)) {
		// Actual local category already exists, can reuse it,
		// and just create a new mapping.
	    } else {
		int64_t x_lcat = handle_alloc();
		if (x_lcat < 0) {
		    warn << "histar_mapcreate: cannot allocate handle\n";
		    da.cb(DELIVERY_REMOTE_ERR);
		    return;
		}

		lcat = x_lcat;
	    }
	}

	cm_->drop_later(lcat);

	/*
	 * Perform this check here, to ensure that we have picked up
	 * any privilege (from lms) the user might have granted us but
	 * could not name as part of the message using global categories,
	 * since those mappings don't yet exist.
	 */
	if (!ctx.can_rw(COBJ(mapreq.ct, mapreq.ct))) {
	    warn << "histar_mapcreate: cannot write target ct\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	dj_cat_mapping mapent = cm_->store(gcat, lcat, mapreq.ct);
	mapres.mappings.push_back(mapent);
    }

    dj_message replym;
    replym.to = m.from;
    replym.target = callmsg.return_ep;
    replym.taint = reply_taint;
    replym.catmap = callmsg.return_cm;
    replym.dset = callmsg.return_ds;
    replym.msg = xdr2str(mapres);

    p_->send(replym, m.dset, 0, 0);
    da.cb(DELIVERY_DONE);
    cm_->drop_now();
}
