extern "C" {
#include <inc/stdio.h>
}

#include <inc/privstore.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>
#include <dj/dis.hh>

class histar_catmgr : public catmgr {
 public:
    histar_catmgr() : ps_(start_env->process_grant) {
	jthread_mutex_init(&mu_);
	droptmo_ = 0;
    }

    virtual ~histar_catmgr() {
	if (droptmo_)
	    timecb_remove(droptmo_);
    }

    virtual uint64_t alloc() {
	int64_t cat = handle_alloc();
	if (cat < 0) {
	    fprintf(stderr, "histar_catmgr::alloc: %s!\n", e2s(cat));
	    throw basic_exception("histar_catmgr::alloc: %s", e2s(cat));
	}

	scoped_jthread_lock l(&mu_);
	ps_.store_priv(cat);
	thread_drop_star(cat);

	return cat;
    }

    virtual void release(uint64_t c) {
	scoped_jthread_lock l(&mu_);
	ps_.drop_priv(c);
    }

    virtual void acquire(const label &l, bool droplater, uint64_t e0, uint64_t e1) {
	scoped_jthread_lock lk(&mu_);

	const struct ulabel *ul = l.to_ulabel_const();
	uint64_t nent = ul->ul_nent;
	level_t def = ul->ul_default;
	for (uint64_t i = 0; i < nent; i++) {
	    uint64_t ent = ul->ul_ent[i];
	    level_t l = LB_LEVEL(ent);
	    if (l != def) {
		uint64_t c = LB_HANDLE(ent);
		if (c == e0 || c == e1)
		    continue;

		ps_.fetch_priv(c);
		if (droplater)
		    dropq_.push_back(c);
	    }
	}

	if (droplater && !droptmo_)
	    droptmo_ = delaycb(1, wrap(this, &histar_catmgr::drop));
    }

    virtual void import(const label &l, uint64_t e0, uint64_t e1) {
	scoped_jthread_lock lk(&mu_);

	const struct ulabel *ul = l.to_ulabel_const();
	uint64_t nent = ul->ul_nent;
	for (uint64_t i = 0; i < nent; i++) {
	    uint64_t ent = ul->ul_ent[i];
	    level_t l = LB_LEVEL(ent);
	    if (l == LB_LEVEL_STAR) {
		uint64_t c = LB_HANDLE(ent);
		if (c != e0 && c != e1 && !ps_.has_priv(c))
		    ps_.store_priv(c);
	    }
	}
    }

 private:
    void drop(void) {
	droptmo_ = 0;

	if (!dropq_.size())
	    return;

	label l, c;
	thread_cur_label(&l);
	thread_cur_clearance(&c);

	while (dropq_.size()) {
	    uint64_t ct = dropq_.pop_back();
	    l.set(ct, l.get_default());
	    c.set(ct, c.get_default());
	}

	thread_set_clearance(&c);
	thread_set_label(&l);
    }

    jthread_mutex_t mu_;
    privilege_store ps_;
    vec<uint64_t> dropq_;
    timecb_t *droptmo_;
};

ptr<catmgr>
dj_catmgr()
{
    return New refcounted<histar_catmgr>();
}
