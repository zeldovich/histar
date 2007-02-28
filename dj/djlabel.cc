#include <inc/error.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>

void
djlabel_to_label(const dj_catmap_indexed &m, const dj_label &dl, label *l,
		 dj_catmap_indexed *out)
{
    if (l)
	l->reset(1);

    for (uint32_t i = 0; i < dl.ents.size(); i++) {
	const dj_gcat &gcat = dl.ents[i];
	uint64_t lcat;
	if (!m.g2l(gcat, &lcat, out))
	    throw basic_exception("djlabel_to_label: missing mapping");
	if (l) {
	    if (l->get(lcat) != 1)
		throw basic_exception("djlabel_to_label: duplicate label entry?");
	    l->set(lcat, gcat.integrity ? 0 : 3);
	}
    }
}

void
label_to_djlabel(const dj_catmap_indexed &m, const label &l, dj_label *dl,
		 dj_catmap_indexed *out)
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
	if (!m.l2g(lcat, &gcat, out))
	    throw basic_exception("label_to_djlabel: missing mapping");

	if (gcat.integrity && lv != 0)
	    throw basic_exception("label_to_djlabel: bad level for integrity");
	if (!gcat.integrity && lv != 3)
	    throw basic_exception("level_to_djlabel: bad level for secrecy");

	if (dl)
	    dl->ents.push_back(gcat);
    }
}
