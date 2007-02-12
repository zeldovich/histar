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

saved_privilege::saved_privilege(uint64_t guard, uint64_t h)
    : handle_(h)
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

    int64_t gate_id = sys_gate_create(start_env->proc_container, 0,
				      gl.to_ulabel(), gc.to_ulabel(), gv.to_ulabel(),
				      "saved privilege", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create failed");
    
    gate_ =  COBJ(start_env->proc_container, gate_id);
}

void
saved_privilege::acquire()
{
    label tl, tc;
    thread_cur_label(&tl);
    thread_cur_clearance(&tc);

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

privilege_store::privilege_store(uint64_t h) : root_handle_(h)
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
    std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
    if (i != m_.end()) {
	delete i->second;
	m_.erase(i);
    }
    m_[h] = new saved_privilege(root_handle_, h);
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
    std::map<uint64_t, saved_privilege*>::iterator i = m_.find(h);
    if (i == m_.end())
	throw basic_exception("fetch_priv: cannot find %ld", h);
    delete i->second;
    m_.erase(i);
}
