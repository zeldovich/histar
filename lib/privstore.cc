#include <inc/privstore.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/gateinvoke.hh>
#include <new>

extern "C" {
#include <inc/assert.h>
#include <inc/setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
}

saved_privilege::saved_privilege(uint64_t guard, uint64_t h, uint64_t ct)
    : handle_(h), gate_(), gc_(true)
{
    // XXX
    // This assumes our default label and clearance levels are
    // 1 and 2, respectively.  If this is incorrect, we'd need
    // to actually call thread_cur_label(), thread_cur_clear().
    // Possible to do this as a fallback when sys_gate_create
    // returns an error..

    label gl(1);
    gl.set(h, LB_LEVEL_STAR);

    label gc(2);
    gc.set(h, 3);

    label gv(3);
    gv.set(guard, 0);

    int64_t gate_id = sys_gate_create(ct, 0,
				      gl.to_ulabel(), gc.to_ulabel(),
				      guard ? gv.to_ulabel() : 0,
				      "saved privilege", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create failed");
    
    gate_ = COBJ(ct, gate_id);
}

void
saved_privilege::acquire()
{
    label tl, tc;
    thread_cur_label(&tl);
    thread_cur_clearance(&tc);

    if (tl.get(handle_) == LB_LEVEL_STAR) {
	if (tc.get(handle_) != 3) {
	    tc.set(handle_, 3);
	    error_check(sys_self_set_clearance(tc.to_ulabel()));
	    thread_label_cache_update(&tl, &tc);
	}
	return;
    }

    tl.set(handle_, LB_LEVEL_STAR);
    tc.set(handle_, 3);

    struct jos_jmp_buf jb;
    if (!jos_setjmp(&jb)) {
	struct thread_entry te;
	memset(&te, 0, sizeof(te));
	error_check(sys_self_get_as(&te.te_as));
	te.te_entry = (void *) &jos_longjmp;
	te.te_arg[0] = (uintptr_t) &jb;
	te.te_arg[1] = 1;

	int r = sys_gate_enter(gate_, tl.to_ulabel(), tc.to_ulabel(), &te);
	throw error(r, "saved_privilege::acquire: sys_gate_enter");
    }

    thread_label_cache_update(&tl, &tc);
}

privilege_store::privilege_store(uint64_t h) : root_handle_(h), m_()
{
}

privilege_store::~privilege_store()
{
    for (std::map<uint64_t, saved_privilege*>::iterator i = m_.begin();
	 i != m_.end(); i++)
    {
	delete i->second;
    }
}

void
privilege_store::store_priv(uint64_t h)
{
    std::map<uint64_t, uint64_t>::iterator ri = refcount_.find(h);
    if (ri != refcount_.end()) {
	uint64_t newref = ri->second + 1;
	refcount_.erase(ri);
	refcount_[h] = newref;
	return;
    }

    assert(m_.find(h) == m_.end());
    m_[h] = new saved_privilege(root_handle_, h, start_env->proc_container);
    refcount_[h] = 1;
}

void 
privilege_store::fetch_priv(uint64_t h)
{
    std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
    if (i == m_.end())
	throw basic_exception("fetch_priv: cannot find %ld", h);
    i->second->acquire();
}

void
privilege_store::drop_priv(uint64_t h)
{
    std::map<uint64_t, uint64_t>::iterator ri = refcount_.find(h);
    if (ri == refcount_.end())
	throw basic_exception("fetch_priv: cannot find %ld", h);

    uint64_t newref = ri->second - 1;
    refcount_.erase(ri);
    if (newref > 0) {
	refcount_[h] = newref;
    } else {
	std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
	assert(i != m_.end());
	delete i->second;
	m_.erase(i);
    }
}

bool
privilege_store::has_priv(uint64_t h)
{
    std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
    if (i == m_.end())
	return false;
    return true;
}
