#include <inc/labelutil.hh>
#include <dj/djutil.hh>
#include <dj/djautorpc.hh>
#include <dj/internalx.h>

enum { map_debug = 0 };

void
dj_map_and_delegate(uint32_t ncat, uint64_t *lcatp, bool *integrityp,
		    const label &grant_local, const label &grant_remote,
		    uint64_t lct, uint64_t rct, const dj_pubkey &host,
		    gate_sender *gs, dj_global_cache &cache,
		    dj_cat_mapping *lmapp, dj_cat_mapping *rmapp,
		    dj_stmt_signed *delegationp)
{
    label taint;
    thread_cur_label(&taint);
    taint.transform(label::star_to, taint.get_default());

    label xgrant = grant_local;
    for (uint32_t i = 0; i < ncat; i++)
	xgrant.set(lcatp[i], LB_LEVEL_STAR);

    dj_pubkey localkey = gs->hostkey();
    dj_autorpc arpc_local(gs, 5, localkey, cache);
    dj_autorpc arpc_remote(gs, 5, host, cache);

    /*
     * Create mapping on local host.
     */
    dj_delivery_code c;
    dj_message_endpoint ep_mapcreate;
    ep_mapcreate.set_type(EP_MAPCREATE);

    dj_mapcreate_arg maparg;
    dj_mapcreate_res mapres;
    label call_taint(taint), call_grant(grant_local), call_clear(taint);

    maparg.reqs.setsize(ncat);

    if (lct) {
	for (uint32_t i = 0; i < ncat; i++) {
	    maparg.reqs[i].ct = lct;
	    maparg.reqs[i].lcat = lcatp[i];
	    maparg.reqs[i].gcat.integrity = integrityp[i] ? 1 : 0;
	}

	/* If a local mapping is already in our cache, force dj_autorpc to
	 * include it in the catmap by granting it.  As a result, mapcreate
	 * will reuse the same global category name for it.
	 */
	for (uint32_t i = 0; i < ncat; i++) {
	    if (cache[localkey]->cmi_.l2g(lcatp[i], 0))
		call_grant.set(lcatp[i], LB_LEVEL_STAR);
	}

	if (map_debug)
	    warn << "map_and_delegate: creating local mapping\n";

	c = arpc_local.call(ep_mapcreate, maparg, mapres,
			    &call_taint, &call_grant, &call_clear, &xgrant);
	if (map_debug)
	    warn << "map_and_delegate: creating local mapping: " << c << "\n";
	if (c != DELIVERY_DONE)
	    throw basic_exception("Could not create local mapping: code %d", c);

	for (uint32_t i = 0; i < ncat; i++) {
	    lmapp[i] = mapres.mappings[i];
	    cache[localkey]->cmi_.insert(lmapp[i]);
	}
    }

    /*
     * Create delegation to the remote host.
     */
    dj_message_endpoint ep_delegate;
    ep_delegate.set_type(EP_DELEGATOR);

    dj_delegate_arg darg;
    darg.reqs.setsize(ncat);    
    for (uint32_t i = 0; i < ncat; i++) {
	darg.reqs[i].gcat = lmapp[i].gcat;
	darg.reqs[i].to = host;
	darg.reqs[i].from_ts = 0;
	darg.reqs[i].until_ts = ~0;
    }

    call_taint = taint; call_grant = grant_local; call_clear = taint;
    for (uint32_t i = 0; i < ncat; i++)
	call_grant.set(lcatp[i], LB_LEVEL_STAR);

    if (map_debug)
	warn << "map_and_delegate: creating delegation\n";
    dj_delegate_res dres;
    c = arpc_local.call(ep_delegate, darg, dres,
			&call_taint, &call_grant, &call_clear);
    if (map_debug)
	warn << "map_and_delegate: creating delegation: " << c << "\n";
    if (c != DELIVERY_DONE)
	throw basic_exception("Could not create delegation: code %d", c);

    for (uint32_t i = 0; i < ncat; i++) {
	delegationp[i] = dres.delegations[i];
	cache.dmap_.insert(delegationp[i]);
    }

    /*
     * Create mapping on remote host.
     */
    for (uint32_t i = 0; i < ncat; i++) {
	maparg.reqs[i].ct = rct;
	maparg.reqs[i].lcat = 0;
	maparg.reqs[i].gcat = lmapp[i].gcat;
    }

    call_taint = taint; call_grant = grant_remote; call_clear = taint;

    /*
     * If we already have a mapping to a local category on the remote
     * machine, force dj_autorpc to include it in the catmap, by granting
     * it, so mapcreate will reuse the same remote category name.
     */
    for (uint32_t i = 0; i < ncat; i++)
	if (cache[host]->cmi_.g2l(lmapp[i].gcat, 0))
	    call_grant.set(lcatp[i], LB_LEVEL_STAR);

    if (map_debug)
	warn << "map_and_delegate: creating remote mapping\n";
    c = arpc_remote.call(ep_mapcreate, maparg, mapres,
			 &call_taint, &call_grant, &call_clear, &xgrant);
    if (map_debug)
	warn << "map_and_delegate: creating remote mapping: " << c << "\n";
    if (c != DELIVERY_DONE)
	throw basic_exception("Could not create remote mapping: code %d", c);

    for (uint32_t i = 0; i < ncat; i++) {
	rmapp[i] = mapres.mappings[i];
	cache[host]->cmi_.insert(rmapp[i]);
    }
}
