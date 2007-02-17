#ifndef JOS_DJ_CATMAP_HH
#define JOS_DJ_CATMAP_HH

#include <itree.h>
#include <dj/dj.h>
#include <dj/djops.hh>

class catmap {
 public:
    catmap() {}
    ~catmap() { l2g_.deleteall(); }

    bool g2l(dj_gcat gcat, uint64_t *lcatp) {
	entry *e = g2l_[gcat];
	if (e && e->global == gcat) {
	    if (lcatp)
		*lcatp = e->local;
	    return true;
	}

	return false;
    }

    bool l2g(uint64_t lcat, dj_gcat *gcatp) {
	entry *e = l2g_[lcat];
	if (e && e->local == lcat) {
	    if (gcatp)
		*gcatp = e->global;
	    return true;
	}

	return false;
    }

    void insert(uint64_t lcat, dj_gcat gcat) {
	entry *e = New entry();
	e->local = lcat;
	e->global = gcat;
	l2g_.insert(e);
	g2l_.insert(e);
    }

 private:
    struct entry {
	itree_entry<entry> llink;
	itree_entry<entry> glink;
	uint64_t local;
	dj_gcat global;
    };

    itree<uint64_t, entry, &entry::local, &entry::llink> l2g_;
    itree<dj_gcat, entry, &entry::global, &entry::glink> g2l_;

    catmap(const catmap&);
    catmap &operator=(const catmap&);
};

#endif
