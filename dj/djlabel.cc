#include <dj/djops.hh>
#include <dj/djlabel.hh>
#include <inc/error.hh>
#include <inc/cpplabel.hh>

void
djlabel_to_label(const dj_catmap_indexed &m, const dj_label &dl, label *l,
		 label_type t, bool skip_missing, dj_catmap_indexed *out)
{
    if (l) {
	if (t == label_taint)
	    l->reset(1);
	else if (t == label_clear)
	    l->reset(0);
	else if (t == label_owner)
	    l->reset(3);
	else
	    throw basic_exception("djlabel_to_label: bad type");
    }

    for (uint32_t i = 0; i < dl.ents.size(); i++) {
	const dj_gcat &gcat = dl.ents[i];
	uint64_t lcat;
	if (!m.g2l(gcat, &lcat, out)) {
	    if (skip_missing)
		continue;
	    warn << "djlabel_to_label: missing mapping for " << gcat << "\n";
	    throw basic_exception("djlabel_to_label: missing mapping");
	}

	if (l) {
	    if (l->get(lcat) != l->get_default())
		throw basic_exception("djlabel_to_label: duplicate label entry?");
	    level_t lv;
	    if (t == label_taint)
		lv = gcat.integrity ? 0 : 3;
	    else if (t == label_clear)
		lv = 3;
	    else if (t == label_owner)
		lv = LB_LEVEL_STAR;
	    else
		throw basic_exception("djlabel_to_label: bad type");
	    l->set(lcat, lv);
	}
    }
}

void
label_to_djlabel(const dj_catmap_indexed &m, const label &l, dj_label *dl,
		 label_type t, bool skip_missing, dj_catmap_indexed *out)
{
    if (dl)
	dl->ents.setsize(0);

    const ulabel *ul = l.to_ulabel_const();
    for (uint32_t i = 0; i < ul->ul_nent; i++) {
	uint64_t ent = ul->ul_ent[i];
	level_t lv = LB_LEVEL(ent);
	if (lv == l.get_default())
	    continue;

	uint64_t lcat = LB_HANDLE(ent);
	dj_gcat gcat;
	if (!m.l2g(lcat, &gcat, out)) {
	    if (skip_missing)
		continue;
	    warn << "label_to_djlabel: missing mapping for " << lcat << "\n";
	    throw basic_exception("label_to_djlabel: missing mapping");
	}

	if (t == label_taint) {
	    if ((gcat.integrity && lv != 0) || (!gcat.integrity && lv != 3))
		throw basic_exception("label_to_djlabel: bad taint level");
	} else if (t == label_clear) {
	    if (lv != 3)
		throw basic_exception("label_to_djlabel: bad clearance level");
	} else if (t == label_owner) {
	    if (lv != LB_LEVEL_STAR)
		throw basic_exception("label_to_djlabel: bad ownership level");
	} else {
	    throw basic_exception("label_to_djlabel: bad label type");
	}

	if (dl)
	    dl->ents.push_back(gcat);
    }
}

dj_catmap_indexed::dj_catmap_indexed(const dj_catmap &cm)
{
    for (uint32_t i = 0; i < cm.ents.size(); i++) {
	const dj_cat_mapping &e = cm.ents[i];
	insert(e);
    }
}

dj_catmap
dj_catmap_indexed::to_catmap()
{
    dj_catmap cm;

    entry *e = g2l_.first();
    while (e) {
	cm.ents.push_back(e->m);
	e = g2l_.next(e);
    }

    return cm;
}

bool
dj_catmap_indexed::g2l(const dj_gcat &gcat, uint64_t *lcatp,
		       dj_catmap_indexed *out) const
{
    entry *e = g2l_[gcat];
    if (e) {
	if (lcatp)
	    *lcatp = e->local;
	if (out)
	    out->insert(e->m);
	return true;
    }

    return false;
}

bool
dj_catmap_indexed::l2g(uint64_t lcat, dj_gcat *gcatp,
		       dj_catmap_indexed *out) const
{
    entry *e = l2g_[lcat];
    if (e) {
	if (gcatp)
	    *gcatp = e->global;
	if (out)
	    out->insert(e->m);
	return true;
    }

    return false;
}

void
dj_catmap_indexed::insert(const dj_cat_mapping &m)
{
    if (g2l(m.gcat, 0) && l2g(m.lcat, 0))
	return;

    entry *e = New entry();
    e->local = m.lcat;
    e->global = m.gcat;
    e->m = m;
    l2g_.insert(e);
    g2l_.insert(e);
}
