#include <inc/error.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>

void
djlabel_to_label(const dj_catmap_indexed &m, const dj_label &dl, label *l)
{
    l->reset(dl.deflevel);
    for (uint32_t i = 0; i < dl.ents.size(); i++) {
	const dj_label_entry &e = dl.ents[i];
	uint64_t lcat;
	if (!m.g2l(e.cat, &lcat))
	    throw basic_exception("djlabel_to_label: missing mapping");
	if (l->get(lcat) != dl.deflevel)
	    throw basic_exception("djlabel_to_label: duplicate label entry?");
	if (e.level > LB_LEVEL_STAR)
	    throw basic_exception("djlabel_to_label: bad level");

	l->set(lcat, e.level);
    }
}
