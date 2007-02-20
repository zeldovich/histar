#ifndef JOS_DJ_REQCONTEXT_HH
#define JOS_DJ_REQCONTEXT_HH

extern "C" {
#include <inc/container.h>
}

#include <async.h>
#include <inc/cpplabel.hh>

class request_context {
 public:
    virtual bool can_read(cobj_ref o) = 0;
    virtual bool can_rw(cobj_ref o) = 0;
    virtual void read_seg(cobj_ref o, str *buf) = 0;
};

class verify_label_reqctx {
 public:
    verify_label_reqctx(const label &vl, const label &vc)
	: vl_(vl), vc_(vc) {}

    virtual bool can_read(cobj_ref o);
    virtual bool can_rw(cobj_ref o);
    virtual void read_seg(cobj_ref o, str *buf);

 private:
    label vl_;
    label vc_;
};

#endif
