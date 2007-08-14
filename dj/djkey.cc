#include <dj/djkey.hh>
#include <dj/perf.hh>

static bool
verify_sign(const dj_stmt &stmt, const dj_pubkey &pk, const dj_sign &sig)
{
    str msg = xdr2str(stmt);
    if (!msg)
	return false;

    ptr<sfspub> p = sfscrypt.alloc(pk, SFS_VERIFY);
    return p && p->verify(sig, msg);
}

bool
verify_stmt(const dj_stmt_signed &s)
{
    PERF_COUNTER(verify_stmt);

    switch (s.stmt.type) {
    case STMT_DELEGATION:
	if (s.stmt.delegation->via)
	    return verify_sign(s.stmt, *s.stmt.delegation->via, s.sign);

	switch (s.stmt.delegation->b.type) {
	case ENT_PUBKEY:
	    return verify_sign(s.stmt, *s.stmt.delegation->b.key, s.sign);

	case ENT_GCAT:
	    return verify_sign(s.stmt, s.stmt.delegation->b.gcat->key, s.sign);

	case ENT_ADDRESS:
	    printf("verify_stmt: cannot speak for a network address\n");
	    return false;

	default:
	    return false;
	}

    case STMT_MSG:
	return verify_sign(s.stmt, s.stmt.msg->from, s.sign);

    case STMT_KEY_SETUP:
	return verify_sign(s.stmt, s.stmt.keysetup->sender, s.sign);

    case STMT_HOSTINFO:
	return verify_sign(s.stmt, s.stmt.info->host, s.sign);

    default:
	return false;
    }
}

bool
key_speaks_for(const dj_pubkey &k, const dj_gcat &gcat,
	       dj_delegation_map &dm, uint32_t depth)
{
    PERF_COUNTER(key_speaks_for);

    if (gcat.key == k)
	return true;

    if (depth == 0)
	return false;

    dj_delegation_map::dm_ent *e = dm.t_[k];
    while (e && e->pk == k) {
	if (e->d.via && !key_speaks_for(*e->d.via, gcat, dm, depth - 1)) {
	    e = dm.t_.next(e);
	    continue;
	}

	if (e->d.b.type == ENT_GCAT)
	    if (*e->d.b.gcat == gcat)
		return true;

	if (e->d.b.type == ENT_PUBKEY)
	    if (key_speaks_for(*e->d.b.key, gcat, dm, depth - 1))
		return true;

	e = dm.t_.next(e);
    }

    return false;
}

void
dj_delegation_map::insert(const dj_delegation_set &dset)
{
    for (uint32_t i = 0; i < dset.ents.size(); i++) {
	dj_stmt_signed ss;
	if (!bytes2xdr(ss, dset.ents[i])) {
	    warn << "dj_delegation_map insert: cannot decode dset entry\n";
	    continue;
	}
	insert(ss);
    }
}

void
dj_delegation_map::insert(const dj_delegation_map &dmap)
{
    for (dm_ent *e = dmap.t_.first(); e; e = dmap.t_.next(e))
	insert(e->ss);
}

void
dj_delegation_map::insert(const dj_stmt_signed &ss)
{
    if (!verify_stmt(ss)) {
	warn << "dj_delegation_map: cannot verify\n";
	return;
    }

    if (ss.stmt.type != STMT_DELEGATION) {
	warn << "dj_delegation_map: not a delegation\n";
	return;
    }

    if (ss.stmt.delegation->a.type != ENT_PUBKEY) {
	warn << "dj_delegation_map: A not a public key\n";
	return;
    }

    str mysig = xdr2str(ss.sign);
    for (dm_ent *e = t_[*ss.stmt.delegation->a.key]; e; e = t_.next(e)) {
	str esig = xdr2str(e->ss.sign);
	if (esig == mysig)
	    return;
    }

    dm_ent *e = New dm_ent(ss);
    t_.insert(e);
    size_++;
}

dj_delegation_set
dj_delegation_map::to_delegation_set()
{
    dj_delegation_set dset;
    for (dm_ent *e = t_.first(); e; e = t_.next(e)) {
	rpc_bytes<2147483647ul> s;
	xdr2bytes(s, e->ss);
	dset.ents.push_back(s);
    }
    return dset;
}
