#include <inc/labelutil.hh>
#include <dj/djutil.hh>
#include <dj/djautorpc.hh>
#include <dj/internalx.h>

void
dj_map_and_delegate(uint64_t lcat, bool integrity,
		    const label &grant_local, const label &grant_remote,
		    uint64_t lct, uint64_t rct, const dj_pubkey &host,
		    gate_sender *gs, dj_global_cache &cache,
		    dj_cat_mapping *lmap,
		    dj_cat_mapping *rmap,
		    dj_stmt_signed *delegation)
{
    label taint;
    thread_cur_label(&taint);
    taint.transform(label::star_to, taint.get_default());

    label xgrant = grant_local;
    xgrant.set(lcat, LB_LEVEL_STAR);

    dj_pubkey localkey = gs->hostkey();
    dj_autorpc arpc_local(gs, 5, localkey, cache);
    dj_autorpc arpc_remote(gs, 5, host, cache);

    /*
     * Create mapping on local host.
     */
    dj_delivery_code c;
    dj_message_endpoint ep_mapcreate;
    ep_mapcreate.set_type(EP_MAPCREATE);

    dj_mapreq mapreq;
    mapreq.ct = lct;
    mapreq.lcat = lcat;
    mapreq.gcat.integrity = integrity ? 1 : 0;

    label call_taint(taint), call_grant(grant_local), call_clear(taint);

    /* If a local mapping is already in our cache, force dj_autorpc to
     * include it in the catmap by granting it.  As a result, mapcreate
     * will reuse the same global category name for it.
     */
    if (cache[localkey]->cmi_.l2g(lcat, 0))
	call_grant.set(lcat, LB_LEVEL_STAR);

    c = arpc_local.call(ep_mapcreate, mapreq, *lmap,
			&call_taint, &call_grant, &call_clear, &xgrant);
    if (c != DELIVERY_DONE)
	throw basic_exception("Could not create local mapping: code %d", c);
    cache[localkey]->cmi_.insert(*lmap);

    /*
     * Create delegation to the remote host.
     */
    dj_message_endpoint ep_delegate;
    ep_delegate.set_type(EP_DELEGATOR);

    dj_delegate_req dreq;
    dreq.gcat = lmap->gcat;
    dreq.to = host;
    dreq.from_ts = 0;
    dreq.until_ts = ~0;

    call_taint = taint; call_grant = grant_local; call_clear = taint;
    call_grant.set(lcat, LB_LEVEL_STAR);
    c = arpc_local.call(ep_delegate, dreq, *delegation,
			&call_taint, &call_grant, &call_clear);
    if (c != DELIVERY_DONE)
	throw basic_exception("Could not create delegation: code %d", c);
    cache.dmap_.insert(*delegation);

    /*
     * Create mapping on remote host.
     */
    mapreq.ct = rct;
    mapreq.lcat = 0;
    mapreq.gcat = lmap->gcat;

    call_taint = taint; call_grant = grant_remote; call_clear = taint;

    /*
     * If we already have a mapping to a local category on the remote
     * machine, force dj_autorpc to include it in the catmap, by granting
     * it, so mapcreate will reuse the same remote category name.
     */
    if (cache[host]->cmi_.g2l(lmap->gcat, 0))
	call_grant.set(lcat, LB_LEVEL_STAR);

    c = arpc_remote.call(ep_mapcreate, mapreq, *rmap,
			 &call_taint, &call_grant, &call_clear, &xgrant);
    if (c != DELIVERY_DONE)
	throw basic_exception("Could not create remote mapping: code %d", c);
    cache[host]->cmi_.insert(*rmap);
}
