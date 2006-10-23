extern "C" {
#include <inc/gateparam.h>
#include <inc/assert.h>
#include <inc/dis/catdir.h>
#include <inc/dis/share.h>

#include <string.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

#include <inc/dis/labeltrans.hh>
#include <inc/dis/shareutils.hh>

label_trans::label_trans(struct cobj_ref catdir_sg)
    : catdir_sg_(catdir_sg), catdir_(0)
{
    client_gt_ = COBJ(0, 0);
    error_check(catdir_map(catdir_sg_, SEGMAP_READ, &catdir_, 0));
}

label_trans::label_trans(struct catdir *dir)
    : catdir_(dir)
{
    catdir_sg_ = COBJ(0, 0);
    client_gt_ = COBJ(0, 0);
    error_check(catdir_map(catdir_sg_, SEGMAP_READ, &catdir_, 0));
}

label_trans::~label_trans(void)
{
    if (catdir_sg_.object)
	segment_unmap_delayed(catdir_, 1);
}

void
label_trans::client_is(struct cobj_ref client_gt)
{
    client_gt_ = client_gt;
}

void
label_trans::local_for(global_label *gl, label *l)
{
    const label *x = gl->local_label(get_local, catdir_);
    l->copy_from(x);
}

void
label_trans::local_for(global_label *gl, label *l, 
		       global_to_local fn, void *arg)
{
    const label *x = gl->local_label(fn, arg);
    l->copy_from(x);
}

int64_t
label_trans::get_local(global_cat *gcat, void *arg)
{
    uint64_t local;
    error_check(catdir_lookup_local((struct catdir *)arg, gcat, &local));
    return local;
}

void
label_trans::localize(const global_label *gl)
{
    struct gate_call_data *d = (gate_call_data *) tls_gate_args;
    struct gate_call_data c;
    gate_call_data_copy_all(&c, d);
    
    struct share_args sargs;
    sargs.op = share_localize;
    assert(sizeof(sargs.localize.label) >= gl->serial_len());
    memcpy(sargs.localize.label, gl->serial(), gl->serial_len());
    int r = gate_send(client_gt_, &sargs, sizeof(sargs), 0);
    gate_call_data_copy_all(d, &c);
    error_check(r);
    error_check(sargs.ret);    
}
