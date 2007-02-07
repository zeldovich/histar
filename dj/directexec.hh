#ifndef JOS_DJ_DIRECTEXEC_HH
#define JOS_DJ_DIRECTEXEC_HH

#include <qhash.h>
#include <dj/dis.hh>
#include <dj/djops.hh>

class dj_direct_gatemap {
 public:
    ptr<djcallexec> newexec(djprot::call_reply_cb);
    qhash<cobj_ref, djgate_service_cb> gatemap_;
};

#endif
