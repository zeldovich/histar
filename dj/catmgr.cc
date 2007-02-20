extern "C" {
#include <inc/stdio.h>
}

#include <inc/privstore.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>
#include <dj/catmgr.hh>
#include <dj/checkpoint.hh>
#include <dj/djops.hh>
#include <qhash.h>

enum { gc_period = 60 };

class histar_catmgr : public catmgr {
 public:
    histar_catmgr() : ps_(start_env->process_grant) {
	jthread_mutex_init(&rd_mu_);
	jthread_mutex_init(&wr_mu_);
	droptmo_ = 0;

	gc_ = delaycb(gc_period, wrap(this, &histar_catmgr::resource_gc));
    }

    virtual ~histar_catmgr() {
	if (droptmo_)
	    timecb_remove(droptmo_);
	timecb_remove(gc_);
    }

    virtual void acquire(const dj_catmap &m, bool droplater) {
	scoped_jthread_lock rl(&rd_mu_);

	for (uint32_t i = 0; i < m.ents.size(); i++) {
	    const dj_cat_mapping &e = m.ents[i];
	    cobj_ref o = COBJ(e.res_ct, e.res_id);
	    dj_cat_mapping *e2 = resmap_[o];
	    if (!e2 || e.lcat != e2->lcat || e.gcat != e2->gcat)
		throw basic_exception("dj_catmap mismatch");

	    ps_.fetch_priv(e.lcat);
	    if (droplater)
		dropq_.push_back(e.lcat);
	}

	if (droplater && !droptmo_)
	    droptmo_ = delaycb(1, wrap(this, &histar_catmgr::drop));
    }

    virtual void resource_check(request_context *ctx, const dj_catmap &m) {
	scoped_jthread_lock rl(&rd_mu_);

	for (uint32_t i = 0; i < m.ents.size(); i++) {
	    const dj_cat_mapping &e = m.ents[i];
	    cobj_ref o = COBJ(e.res_ct, e.res_id);
	    if (!ctx->can_read(o))
		throw basic_exception("dj_catmap dead resource");
	}
    }

    /*
     * XXX
     *
     * This needs to be figured out...
     */
    virtual uint64_t alloc() {
	int64_t cat = handle_alloc();
	if (cat < 0) {
	    fprintf(stderr, "histar_catmgr::alloc: %s!\n", e2s(cat));
	    throw basic_exception("histar_catmgr::alloc: %s", e2s(cat));
	}

	scoped_jthread_lock wl(&wr_mu_);
	scoped_jthread_lock rl(&rd_mu_);
	ps_.store_priv(cat);
	thread_drop_star(cat);

	rl.release();
	checkpoint_update();
	return cat;
    }

 private:
    void resource_gc() {
	gc_ = delaycb(gc_period, wrap(this, &histar_catmgr::resource_gc));

	/*
	 * XXX not implemented yet
	 *
	 * Iterate over reslabel_, acquiring the appropriate privilege
	 * for each resource, checking that it still exists, and if not,
	 * dropping it from reslabel_, resmap_, and ps_.
	 */
    }

    void drop() {
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

    jthread_mutex_t wr_mu_;
    jthread_mutex_t rd_mu_;
    privilege_store ps_;
    vec<uint64_t> dropq_;
    qhash<cobj_ref, dj_cat_mapping> resmap_;
    qhash<cobj_ref, label> reslabel_;
    timecb_t *droptmo_;
    timecb_t *gc_;
};

catmgr*
catmgr::alloc()
{
    return New histar_catmgr();
}
