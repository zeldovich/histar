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
#include <inttypes.h>
}

void 
saved_privilege::init(uint64_t guard, uint64_t c, uint64_t c2, uint64_t ct)
{
    label lowner;
    lowner.add(c);

    if (c2)
	lowner.add(c2);

    label lclear;

    label lguard;
    lguard.add(guard);

    int64_t gate_id = sys_gate_create(ct, 0,
				      0, lowner.to_ulabel(), lclear.to_ulabel(),
				      guard ? lguard.to_ulabel() : 0,
				      "saved privilege");
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create failed");
    
    gate_ = COBJ(ct, gate_id);
}

saved_privilege::saved_privilege(uint64_t guard, uint64_t h, uint64_t ct)
    : category_(h), category2_(0), gate_(), gc_(true)
{
    init(guard, h, 0, ct);
}

saved_privilege::saved_privilege(uint64_t guard, uint64_t h, uint64_t h2, uint64_t ct)
    : category_(h), category2_(h2), gate_(), gc_(true)
{
    init(guard, h, h2, ct);
}

void
saved_privilege::acquire()
{
    label to, tc;
    thread_cur_ownership(&to);
    thread_cur_clearance(&tc);

    to.add(category_);
    if (category2_)
	to.add(category2_);

    error_check(sys_gate_enter(gate_, to.to_ulabel(), tc.to_ulabel(), 1));
    thread_label_cache_update(0, &to, 0);
}

privilege_store::privilege_store(uint64_t h) : root_category_(h), m_()
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
    m_[h] = new saved_privilege(root_category_, h, start_env->proc_container);
    refcount_[h] = 1;
}

void 
privilege_store::fetch_priv(uint64_t h)
{
    std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
    if (i == m_.end())
	throw basic_exception("fetch_priv: cannot find %"PRIu64, h);
    i->second->acquire();
}

void
privilege_store::drop_priv(uint64_t h)
{
    std::map<uint64_t, uint64_t>::iterator ri = refcount_.find(h);
    if (ri == refcount_.end())
	throw basic_exception("fetch_priv: cannot find %"PRIu64, h);

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
