#ifndef JOS_DJ_DJCACHE_HH
#define JOS_DJ_DJCACHE_HH

#include <dj/djlabel.hh>
#include <dj/djops.hh>
#include <qhash.h>

class dj_node_cache {
 public:
    dj_node_cache() {}
    void insert(const dj_catmap &m);

    dj_catmap_indexed cmi_;
};

class dj_global_cache {
 public:
    dj_global_cache() {}
    ~dj_global_cache() { m_.traverse(wrap(&dj_global_cache::delete_nc)); }

    dj_node_cache *home() {
	return &home_;
    }

    dj_node_cache *get(const dj_pubkey &pk) {
	if (!m_[pk])
	    m_.insert(pk, New dj_node_cache());
	return *m_[pk];
    }

    dj_node_cache *operator[](const dj_pubkey &pk) {
	return get(pk);
    }

 private:
    static void delete_nc(const dj_pubkey&, dj_node_cache **nc) {
	delete *nc;
    }

    dj_node_cache home_;
    qhash<dj_pubkey, dj_node_cache*> m_;
};

#endif
