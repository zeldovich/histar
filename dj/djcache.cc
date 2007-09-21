#include <dj/djcache.hh>

void
dj_node_cache::insert(const dj_catmap &cm)
{
    for (uint32_t i = 0; i < cm.ents.size(); i++)
	cmi_.insert(cm.ents[i]);
}
