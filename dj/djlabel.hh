#ifndef JOS_DJ_DJLABEL_HH
#define JOS_DJ_DJLABEL_HH

#include <ihash.h>
#include <dj/djprotx.h>
#include <dj/djops.hh>
#include <inc/cpplabel.hh>

class dj_catmap_indexed {
 public:
    dj_catmap_indexed() {}
    dj_catmap_indexed(const dj_catmap &cm);
    ~dj_catmap_indexed() { l2g_.deleteall(); }

    dj_catmap to_catmap();
    bool g2l(const dj_gcat &gcat, uint64_t *lcatp,
	     dj_catmap_indexed *out = 0) const;
    bool l2g(uint64_t lcat, dj_gcat *gcatp,
	     dj_catmap_indexed *out = 0) const;
    void insert(const dj_cat_mapping &m);
    void insert(const dj_catmap &cm);

 private:
    struct entry {
	ihash_entry<entry> llink;
	ihash_entry<entry> glink;
	uint64_t local;
	dj_gcat global;
	dj_cat_mapping m;
    };

    ihash<uint64_t, entry, &entry::local, &entry::llink> l2g_;
    ihash<dj_gcat, entry, &entry::global, &entry::glink> g2l_;

    dj_catmap_indexed(const dj_catmap_indexed&);
    dj_catmap_indexed &operator=(const dj_catmap_indexed&);
};

typedef enum { label_taint, label_clear, label_owner } label_type;

void label_to_djlabel(const dj_catmap_indexed&, const label&, dj_label*,
		      label_type, bool skip_missing = false, dj_catmap_indexed *out = 0);
void djlabel_to_label(const dj_catmap_indexed&, const dj_label&, label*,
		      label_type, bool skip_missing = false, dj_catmap_indexed *out = 0);

#endif
