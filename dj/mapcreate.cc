#include <inc/labelutil.hh>
#include <inc/privstore.hh>
#include <inc/scopeguard.hh>
#include <dj/mapcreate.hh>
#include <dj/mapcreatex.h>
#include <dj/djlabel.hh>
#include <dj/djops.hh>
#include <dj/djrpcx.h>

void
histar_mapcreate::exec(const dj_pubkey &sender, const dj_message &m,
		       const delivery_args &da)
{
    if (m.target.type != EP_MAPCREATE) {
	warn << "histar_mapcreate: not a mapcreate target\n";
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    label msg_taint;
    try {
	dj_catmap_indexed cmi(m.catmap);
	djlabel_to_label(cmi, m.taint, &msg_taint);
    } catch (std::exception &e) {
	warn << "histar_mapcreate: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING, 0);
	return;
    }

    verify_label_reqctx ctx(msg_taint, msg_taint);
    try {
	cm_->acquire(m.catmap, true);
	cm_->resource_check(&ctx, m.catmap);
    } catch (std::exception &e) {
	warn << "histar_mapcreate: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING, 0);
	return;
    }

    dj_call_msg callmsg;
    dj_mapreq mapreq;
    if (!bytes2xdr(callmsg, m.msg) || !bytes2xdr(mapreq, callmsg.buf)) {
	warn << "histar_mapcreate: cannot unmarshal\n";
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    if (callmsg.return_ep.type != EP_GATE) {
	warn << "histar_mapcreate: must return to a gate\n";
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    if (!ctx.can_rw(COBJ(mapreq.ct, mapreq.ct))) {
	warn << "histar_mapcreate: cannot write target ct\n";
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    dj_cat_mapping mapent;
    if (mapreq.lcat) {		/* Create a global category */
	if (!da.local_delivery_arg) {
	    warn << "histar_mapcreate: missing local_delivery_arg\n";
	    da.cb(DELIVERY_REMOTE_ERR, 0);
	    return;
	}

	label gl;
	cobj_ref privgate = COBJ(start_env->proc_container,
				 da.local_delivery_arg);
	obj_get_label(privgate, &gl);
	if (gl.get(mapreq.lcat) != LB_LEVEL_STAR) {
	    warn << "histar_mapcreate: trying to map unpriv cat\n";
	    da.cb(DELIVERY_REMOTE_ERR, 0);
	    return;
	}

	saved_privilege sp(mapreq.lcat, privgate);
	sp.acquire();
	scope_guard<void, uint64_t> drop(thread_drop_star, mapreq.lcat);

	dj_gcat gcat;
	gcat.key = p_->pubkey();
	gcat.id = ++counter_;

	mapent = cm_->store(gcat, mapreq.lcat, mapreq.ct);
    } else {
	int64_t lcat = handle_alloc();
	if (lcat < 0) {
	    warn << "histar_mapcreate: cannot allocate handle\n";
	    da.cb(DELIVERY_REMOTE_ERR, 0);
	    return;
	}

	scope_guard<void, uint64_t> drop(thread_drop_star, lcat);
	mapent = cm_->store(mapreq.gcat, lcat, mapreq.ct);
    }

    dj_message replym;
    replym.target = callmsg.return_ep;
    replym.msg_ct = callmsg.return_ct;
    replym.token = mapent.res_ct;
    replym.taint = m.taint;
    replym.glabel.deflevel = 3;
    replym.glabel.ents.setsize(1);
    replym.glabel.ents[0].cat = mapent.gcat;
    replym.glabel.ents[0].level = LB_LEVEL_STAR;
    replym.gclear.deflevel = 0;
    replym.gclear.ents.setsize(1);
    replym.gclear.ents[0].cat = mapent.gcat;
    replym.gclear.ents[0].level = 3;
    replym.catmap = callmsg.return_cm;
    replym.dset = callmsg.return_ds;

    if (sender == p_->pubkey())
	replym.catmap.ents.push_back(mapent);

    p_->send(sender, 0, m.dset, replym, 0, 0);
    da.cb(DELIVERY_DONE, replym.token);
}
