#include <dj/djcache.hh>

dj_label
dj_node_cache::label_convert(const label &l, dj_catmap_indexed *cm)
{
    dj_label r;
    label_to_djlabel(cmi_, l, &r, cm);
    return r;
}

void
dj_node_cache::insert(const dj_catmap &cm)
{
    for (uint32_t i = 0; i < cm.ents.size(); i++)
	cmi_.insert(cm.ents[i]);
}
