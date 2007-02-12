extern "C" {
#include <inc/stdio.h>
}

#include <inc/privstore.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <dj/dis.hh>

class histar_catmgr : public catmgr {
 public:
    histar_catmgr() : ps_key_(handle_alloc()), ps_(ps_key_) {}

    virtual uint64_t alloc() {
	int64_t cat = handle_alloc();
	if (cat < 0) {
	    fprintf(stderr, "histar_catmgr::alloc: %s!\n", e2s(cat));
	    throw basic_exception("histar_catmgr::alloc: %s", e2s(cat));
	}

	ps_.store_priv(cat);
	thread_drop_star(cat);

	return cat;
    }

    virtual void release(uint64_t c) {
	ps_.drop_priv(c);
    }

    virtual void acquire(const label &l) {
	const struct ulabel *ul = l.to_ulabel_const();
	uint64_t nent = ul->ul_nent;
	level_t def = ul->ul_default;
	for (uint64_t i = 0; i < nent; i++) {
	    uint64_t ent = ul->ul_ent[i];
	    level_t l = LB_LEVEL(ent);
	    if (l != def) {
		uint64_t c = LB_HANDLE(ent);
		ps_.fetch_priv(c);
	    }
	}
    }

 private:
    uint64_t ps_key_;
    privilege_store ps_;
};

ptr<catmgr>
dj_catmgr()
{
    return New refcounted<histar_catmgr>();
}
