extern "C" {
#include <stdio.h>
}

#include <async.h>
#include <crypt.h>
#include <inc/authclnt.hh>

#include <dj/gatesender.hh>
#include <dj/djautorpc.hh>
#include <dj/internalx.h>
#include <dj/djutil.hh>
#include <dj/miscx.h>

enum { authproxy_debug = 0 };

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
	 * Fill in category mappings & delegations that we will need
	 * to issue MAPCREATE & DELEGATE RPCs.
	 */
	dj_pubkey thiskey = the_gs->hostkey();
	dj_global_cache cache;
	cache[thiskey]->cmi_.insert(m.catmap);
	cache[r->sender]->cmi_.insert(r->msg.catmap);
	cache.dmap_.insert(m.dset);
	cache.dmap_.insert(r->msg.dset);

	/*
	 * Convert caller's labels so we can invoke mapcreate.
	 */
	label taint, glabel, gclear;
	djlabel_to_label(cache[thiskey]->cmi_, m.taint,  &taint,  label_taint);
	djlabel_to_label(cache[thiskey]->cmi_, m.glabel, &glabel, label_owner);
	djlabel_to_label(cache[thiskey]->cmi_, m.gclear, &gclear, label_clear);

	if (authproxy_debug) {
	    warn << "authproxy: taint  " << taint.to_string() << "\n";
	    warn << "authproxy: glabel " << glabel.to_string() << "\n";
	    warn << "authproxy: gclear " << gclear.to_string() << "\n";
	}

	/*
	 * Create mappings & delegations
	 */
	uint64_t cats[2] = { ug, ut };
	bool integrity[2] = { true, false };
	dj_cat_mapping lmap[2], rmap[2];
	dj_stmt_signed delegations[2];

	dj_map_and_delegate(2, &cats[0], &integrity[0],
			    glabel, glabel,
			    arg.map_ct, arg.return_map_ct, r->sender,
			    the_gs, cache,
			    &lmap[0], &rmap[0], &delegations[0]);

	res.resok->ug_local = lmap[0];
	res.resok->ut_local = lmap[1];
	res.resok->ug_remote = rmap[0];
	res.resok->ut_remote = rmap[1];
	res.resok->ug_delegation = delegations[0];
	res.resok->ut_delegation = delegations[1];

	/*
	 * XXX if we cache global category names, esp. those that were
	 * created on a different exporter, we may need to potentially
	 * need to fill in r->msg.dset to prove that our exporter
	 * speaks for them.  since we're only granting privilege that
	 * our exporter inherently speaks for here, it's fine.
	 */
	r->catmap.ents.push_back(res.resok->ug_local);
	r->catmap.ents.push_back(res.resok->ut_local);
	r->msg.catmap.ents.push_back(res.resok->ug_remote);
	r->msg.catmap.ents.push_back(res.resok->ut_remote);

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
    for (uint32_t retry = 0; ; retry++) {
	try {
	    the_gs = New gate_sender();
	    break;
	} catch (...) {
	    if (retry == 10)
		throw;

	    sleep(1);
	}
    }

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "authproxy";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    printf("authproxy: gate %lu.%lu\n", g.container, g.object);
    thread_halt();
}
