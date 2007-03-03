extern "C" {
#include <stdio.h>
}

#include <async.h>
#include <crypt.h>
#include <inc/authclnt.hh>

#include <dj/djgatesrv.hh>
#include <dj/gatesender.hh>
#include <dj/djautorpc.hh>
#include <dj/internalx.h>
#include <dj/miscx.h>

static gate_sender *the_gs;

bool
auth_proxy_service(const dj_message &m, const str &s, dj_rpc_reply *r)
{
    authproxy_arg arg;
    authproxy_res res;

    if (!str2xdr(arg, s)) {
	warn << "auth_proxy: cannot unmarshal\n";
	return false;
    }

    try {
	res.set_ok(true);

	uint64_t ug, ut;
	auth_login(arg.username, arg.password, &ug, &ut);

	/*
	 * Fill in category mappings that we will need soon.
	 */
	dj_pubkey thiskey = the_gs->hostkey();
	dj_global_cache cache;
	cache[thiskey]->cmi_.insert(m.catmap);
	cache[r->sender]->cmi_.insert(r->catmap);

	/*
	 * Convert caller's labels so we can invoke mapcreate.
	 */
	label taint, glabel, gclear;
	djlabel_to_label(cache[thiskey]->cmi_, m.taint,  &taint,  label_taint);
	djlabel_to_label(cache[thiskey]->cmi_, m.glabel, &glabel, label_owner);
	djlabel_to_label(cache[thiskey]->cmi_, m.gclear, &gclear, label_clear);

	glabel.set(ug, LB_LEVEL_STAR);
	glabel.set(ut, LB_LEVEL_STAR);
	gclear.set(ug, 3);
	gclear.set(ut, 3);

	/*
	 * autorpc objects for doing mapcreate & delegate.
	 */
	dj_autorpc local_ar(the_gs, 2, thiskey, cache);
	dj_autorpc remote_ar(the_gs, 2, r->sender, cache);

	/*
	 * Map user categories onto global categories.
	 */
	dj_delivery_code c;
	dj_message_endpoint mapcreate_ep;
	mapcreate_ep.set_type(EP_MAPCREATE);

	dj_mapreq mapreq;
	mapreq.gcat.integrity = 1;
	mapreq.lcat = ug;
	mapreq.ct = arg.map_ct;

	label t(taint), gl(glabel), gc(gclear), xl(3);
	xl.set(ug, LB_LEVEL_STAR);
	c = local_ar.call(mapcreate_ep, mapreq, res.resok->ug_local,
			  &t, &gl, &gc, &xl);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot mapcreate local ug");

	mapreq.gcat.integrity = 0;
	mapreq.lcat = ut;

	t = taint; gl = glabel; gc = gclear; xl.reset(3);
	xl.set(ut, LB_LEVEL_STAR);
	c = local_ar.call(mapcreate_ep, mapreq, res.resok->ut_local,
			  &t, &gl, &gc, &xl);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot mapcreate local ut");

	cache[thiskey]->cmi_.insert(res.resok->ut_local);
	cache[thiskey]->cmi_.insert(res.resok->ug_local);

	/*
	 * Generate delegations.
	 */
	dj_message_endpoint delegate_ep;
	delegate_ep.set_type(EP_DELEGATOR);

	dj_delegate_req dreq;
	dreq.gcat = res.resok->ug_local.gcat;
	dreq.to = r->sender;
	dreq.from_ts = 0;
	dreq.until_ts = ~0;

	xl.reset(3);
	xl.set(ug, LB_LEVEL_STAR);
	c = local_ar.call(delegate_ep, dreq, res.resok->ug_delegation,
			  0, &xl, 0);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot delegate ug");

	dreq.gcat = res.resok->ut_local.gcat;
	xl.reset(3);
	xl.set(ut, LB_LEVEL_STAR);
	c = local_ar.call(delegate_ep, dreq, res.resok->ut_delegation,
			  0, &xl, 0);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot delegate ut");

	cache.dmap_.insert(res.resok->ug_delegation);
	cache.dmap_.insert(res.resok->ut_delegation);

	/*
	 * Create mappings on the remote side.
	 */
	mapreq.gcat = res.resok->ug_local.gcat;
	mapreq.lcat = 0;
	mapreq.ct = arg.return_map_ct;
	t = taint; gl = glabel; gc = gclear;
	c = remote_ar.call(mapcreate_ep, mapreq, res.resok->ug_remote,
			   &t, &gl, &gc);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot mapcreate remote ug");

	mapreq.gcat = res.resok->ut_local.gcat;
	t = taint; gl = glabel; gc = gclear;
	c = remote_ar.call(mapcreate_ep, mapreq, res.resok->ut_remote,
			   &t, &gl, &gc);
	if (c != DELIVERY_DONE)
	    throw basic_exception("cannot mapcreate remote ut");

	/*
	 * We should be done..
	 */
	r->msg.glabel.ents.push_back(res.resok->ug_local.gcat);
	r->msg.glabel.ents.push_back(res.resok->ut_local.gcat);
	r->msg.gclear.ents.push_back(res.resok->ug_local.gcat);
	r->msg.gclear.ents.push_back(res.resok->ut_local.gcat);
    } catch (std::exception &e) {
	warn << "auth_proxy: " << e.what() << "\n";
	res.set_ok(false);
    }

    r->msg.msg = xdr2str(res);
    return true;
}

static void
gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(auth_proxy_service, gcd, r);
}

int
main(int ac, char **av)
{
    gate_sender gs;
    the_gs = &gs;

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "authproxy";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    printf("authproxy: gate %lu.%lu\n", g.container, g.object);
    thread_halt();
}
