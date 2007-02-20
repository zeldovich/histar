#ifndef JOS_DJ_DJLABEL_HH
#define JOS_DJ_DJLABEL_HH

#include <ihash.h>
#include <dj/djprotx.h>
#include <inc/cpplabel.hh>

class dj_catmap_indexed {
 public:
    dj_catmap_indexed(const dj_catmap &cm) {
	for (uint32_t i = 0; i < cm.ents.size(); i++) {
	    const dj_cat_mapping &e = cm.ents[i];
	    insert(e.lcat, e.gcat);
	}
    }

    ~dj_catmap_indexed() { l2g_.deleteall(); }

    bool g2l(const dj_gcat &gcat, uint64_t *lcatp) const {
	entry *e = g2l_[gcat];
	if (e) {
	    if (lcatp)
		*lcatp = e->local;
	    return true;
	}

	return false;
    }

    bool l2g(uint64_t lcat, dj_gcat *gcatp) const {
	entry *e = l2g_[lcat];
	if (e) {
	    if (gcatp)
		*gcatp = e->global;
	    return true;
	}

	return false;
    }

 private:
    void insert(uint64_t lcat, const dj_gcat &gcat) {
	entry *e = New entry();
	e->local = lcat;
	e->global = gcat;
	l2g_.insert(e);
	g2l_.insert(e);
    }

    struct entry {
	ihash_entry<entry> llink;
	ihash_entry<entry> glink;
	uint64_t local;
	dj_gcat global;
    };

    ihash<uint64_t, entry, &entry::local, &entry::llink> l2g_;
    ihash<dj_gcat, entry, &entry::global, &entry::glink> g2l_;

    dj_catmap_indexed(const dj_catmap_indexed&);
    dj_catmap_indexed &operator=(const dj_catmap_indexed&);
};

void label_to_djlabel(const dj_catmap_indexed&, const label&, dj_label*);
void djlabel_to_label(const dj_catmap_indexed&, const dj_label&, label*);

#endif
