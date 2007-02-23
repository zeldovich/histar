#include <inc/error.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>

void
djlabel_to_label(const dj_catmap_indexed &m, const dj_label &dl, label *l,
		 dj_catmap_indexed *out)
{
    if (l)
	l->reset(dl.deflevel);
    for (uint32_t i = 0; i < dl.ents.size(); i++) {
	const dj_label_entry &e = dl.ents[i];
	uint64_t lcat;
	if (e.level == dl.deflevel)
	    continue;
	if (!m.g2l(e.cat, &lcat, out))
	    throw basic_exception("djlabel_to_label: missing mapping");
	if (l && l->get(lcat) != dl.deflevel)
	    throw basic_exception("djlabel_to_label: duplicate label entry?");
	if (e.level > LB_LEVEL_STAR)
	    throw basic_exception("djlabel_to_label: bad level");

	if (l)
	    l->set(lcat, e.level);
    }
}

void
label_to_djlabel(const dj_catmap_indexed &m, const label &l, dj_label *dl,
		 dj_catmap_indexed *out)
{
    if (dl) {
	dl->deflevel = l.get_default();
	dl->ents.setsize(0);
    }

    const ulabel *ul = l.to_ulabel_const();
    for (uint32_t i = 0; i < ul->ul_nent; i++) {
	uint64_t ent = ul->ul_ent[i];
	level_t lv = LB_LEVEL(ent);
	if (lv == l.get_default())
	    continue;

	uint64_t lcat = LB_HANDLE(ent);
	dj_label_entry dje;
	if (!m.l2g(lcat, &dje.cat, out))
	    throw basic_exception("label_to_djlabel: missing mapping");

	dje.level = lv;
	if (dl)
	    dl->ents.push_back(dje);
    }
}
