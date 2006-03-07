#include <inc/privstore.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <new>

saved_privilege::saved_privilege(uint64_t guard, uint64_t h)
    : handle_(h)
{
    label l;
    thread_cur_label(&l);
    l.set(h, LB_LEVEL_STAR);	// make sure we have it

    label clear;
    thread_cur_clearance(&clear);
    clear.set(guard, 0);

    gate_ = new gatesrv(start_env->proc_container, "saved privilege",
			&l, &clear);
    gate_->set_entry_container(start_env->proc_container);
    gate_->set_entry_function(&entry_stub, this);
    gate_->enable();
}

void
saved_privilege::acquire()
{
    gate_call(gate_->gate(), 0, 0, 0, 0);
}

void
saved_privilege::entry_stub(void *arg,
			    struct gate_call_data *gcd,
			    gatesrv_return *r)
{
    saved_privilege *sp = (saved_privilege *) arg;
    sp->entry(r);
}

void
saved_privilege::entry(gatesrv_return *r)
{
    label *ds = new label(3);
    ds->set(handle_, LB_LEVEL_STAR);

    r->ret(0, ds, 0);
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
