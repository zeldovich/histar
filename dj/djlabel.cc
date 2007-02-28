#include <inc/error.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>

void
djlabel_to_label(const dj_catmap_indexed &m, const dj_label &dl, label *l,
		 label_type t, dj_catmap_indexed *out)
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
		 label_type t, dj_catmap_indexed *out)
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
