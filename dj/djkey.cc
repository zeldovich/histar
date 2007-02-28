#include <dj/djkey.hh>

bool
verify_stmt(const dj_stmt_signed &s)
{
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

    case STMT_MSG_XFER:
	return verify_sign(s.stmt, s.stmt.msgx->from, s.sign);

    case STMT_KEY_SETUP:
	return verify_sign(s.stmt, s.stmt.keysetup->sender, s.sign);

    default:
	return false;
    }
}

bool
key_speaks_for(const dj_pubkey &k, const dj_gcat &gcat,
	       dj_delegation_map &dm, uint32_t depth)
{
    if (gcat.key == k)
	return true;

    if (depth == 0)
	return false;

    dj_delegation_map::dm_ent *e = dm.t_[k];
    while (e && e->pk == k) {
	if (e->d.via && !key_speaks_for(*e->d.via, gcat, dm, depth - 1))
	    continue;

	if (e->d.b.type == ENT_GCAT) {
	    if (*e->d.b.gcat == gcat)
		return true;
	}

	if (e->d.b.type == ENT_PUBKEY) {
	    if (key_speaks_for(*e->d.b.key, gcat, dm, depth - 1))
		return true;
	}

	e = dm.t_.next(e);
    }

    return false;
}
