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
    label l;
    thread_cur_label(&l);
    l.set(h, LB_LEVEL_STAR);	// make sure we have it

    label clear;
    thread_cur_clearance(&clear);
    clear.set(guard, 0);

    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &entry;
    te.te_stack = (char *) tls_stack_top - 8;
    error_check(sys_self_get_as(&te.te_as));
    
    int64_t gate_id = sys_gate_create(start_env->proc_container, &te,
				      clear.to_ulabel(),
				      l.to_ulabel(), "saved privilege", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create failed");
    
    gate_ =  COBJ(start_env->proc_container, gate_id);
}

void
saved_privilege::acquire()
{
    static_assert(sizeof(struct jos_jmp_buf) <= sizeof(struct gate_call_data));

    struct jos_jmp_buf *jb = (struct jos_jmp_buf *)tls_gate_args;
    if (!jos_setjmp(jb)) {
	label thread_label, thread_clear;
	label gate_label, gate_clear;
	label tgt_label, tgt_clear;

	thread_cur_label(&thread_label);
	thread_cur_clearance(&thread_clear);

	obj_get_label(gate_, &gate_label);
	gate_get_clearance(gate_, &gate_clear);

	thread_label.merge(&gate_label, &tgt_label, label::min, label::leq_starlo);
	thread_clear.merge(&gate_clear, &tgt_clear, label::max, label::leq_starlo);
		
	gate_invoke(gate_, &tgt_label, &tgt_clear, 0, 0);
    }
}

void __attribute__((noreturn))
saved_privilege::entry(void)
{
    thread_label_cache_invalidate();

    struct jos_jmp_buf *jb = (struct jos_jmp_buf *)tls_gate_args;
    jos_longjmp(jb, 0);
    
    printf("saved_privilege::entry: jos_longjmp returned");
    thread_halt();
}

privilege_store::privilege_store(uint64_t h)
    : root_handle_(h), privsize_(0), privs_(0)
{
}

privilege_store::~privilege_store()
{
    for (uint32_t i = 0; i < privsize_; i++)
	if (privs_[i])
	    delete privs_[i];
}

int
privilege_store::slot_find(uint64_t h)
{
    for (uint32_t i = 0; i < privsize_; i++)
	if (privs_[i] && privs_[i]->handle() == h)
	    return i;
    return -1;
}

int
privilege_store::slot_alloc()
{
    for (uint32_t i = 0; i < privsize_; i++)
	if (!privs_[i])
	    return i;

    int slot = privsize_;

    uint32_t nsize = MAX(privsize_, 8UL) * 2;
    uint32_t nbytes = nsize * sizeof(privs_[0]);
    saved_privilege **nprivs = (saved_privilege **) realloc(privs_, nbytes);
    if (nprivs == 0)
	throw std::bad_alloc();

    for (uint32_t i = privsize_; i < nsize; i++)
	nprivs[i] = 0;
    privsize_ = nsize;
    privs_ = nprivs;

    return slot;
}

void
privilege_store::store_priv(uint64_t h)
{
    int slot = slot_find(h);
    if (slot >= 0)
	throw basic_exception("store_priv: %ld already present", h);

    slot = slot_alloc();
    privs_[slot] = new saved_privilege(root_handle_, h);
}

void 
privilege_store::fetch_priv(uint64_t h)
{
    int slot = slot_find(h);
    if (slot < 0)
	throw basic_exception("fetch_priv: cannot find %ld", h);
    privs_[slot]->acquire();
}
