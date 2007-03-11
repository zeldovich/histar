#include <inc/privstore.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <dj/catmgr.hh>
#include <dj/djops.hh>
#include <dj/perf.hh>

enum { gc_period = 60 };

class histar_catmgr : public catmgr {
 public:
    histar_catmgr() : droptmo_(0) {}

    virtual ~histar_catmgr() {
	drop_now();
    }

    virtual void acquire(const dj_catmap &m, bool droplater) {
	PERF_COUNTER(catmgr::acquire);

	for (uint32_t i = 0; i < m.ents.size(); i++) {
	    const dj_cat_mapping &e = m.ents[i];

	    label ctl;
	    cobj_ref res_cto = COBJ(e.res_ct, e.res_ct);
	    obj_get_label(res_cto, &ctl);
	    if (ctl.get(start_env->process_grant) != 0 ||
		ctl.get(start_env->process_taint) != 3)
		throw basic_exception("badly labeled mapping container");

	    char digest[20];
	    if (!sha1_hashxdr(&digest[0], e))
		throw basic_exception("cannot marshal dj_cat_mapping");

	    char meta[KOBJ_META_LEN];
	    assert(sizeof(meta) >= sizeof(digest));

	    error_check(sys_obj_get_meta(res_cto, &meta[0]));
	    if (memcmp(&meta[0], &digest[0], sizeof(digest)))
		throw basic_exception("dj_cat_mapping hash mismatch");

	    saved_privilege(e.lcat, COBJ(e.res_ct, e.res_gt)).acquire();
	    if (droplater)
		drop_later(e.lcat);
	}
    }

    virtual void drop_later(uint64_t cat) {
	dropq_.push_back(cat);

	if (!droptmo_)
	    droptmo_ = delaycb(10, wrap(this, &histar_catmgr::drop));
    }

    virtual dj_cat_mapping store(const dj_gcat &gc, uint64_t lc, uint64_t uct) {
	PERF_COUNTER(catmgr::store);

	label ctl(1);
	ctl.set(start_env->process_grant, 0);
	ctl.set(start_env->process_taint, 3);

	int64_t rct = sys_container_alloc(uct, ctl.to_ulabel(), "dj_cat_mapping",
					  0, 65536);
	error_check(rct);

	saved_privilege sp(0, lc, rct);

	dj_cat_mapping e;
	e.gcat = gc;
	e.lcat = lc;
	e.res_ct = rct;
	e.res_gt = sp.gate().object;

	char meta[KOBJ_META_LEN];
	if (!sha1_hashxdr(&meta[0], e))
	    throw basic_exception("cannot marshal dj_cat_mapping");
	error_check(sys_obj_set_meta(COBJ(rct, rct), 0, &meta[0]));

	sp.set_gc(false);
	return e;
    }

    virtual void drop_now() {
	if (droptmo_)
	    timecb_remove(droptmo_);
	drop();
    }

 private:
    void drop() {
	PERF_COUNTER(catmgr::drop);

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

    vec<uint64_t> dropq_;
    timecb_t *droptmo_;
};

catmgr*
catmgr::alloc()
{
    return New histar_catmgr();
}
