#ifndef JOS_DJ_DIRECTEXEC_HH
#define JOS_DJ_DIRECTEXEC_HH

#include <qhash.h>
#include <dj/dis.hh>
#include <dj/djops.hh>

class dj_direct_gatemap {
 public:
    dj_direct_gatemap() : gatemap_() {}
    ptr<djcallexec> newexec(djprot::call_reply_cb);
    qhash<cobj_ref, djgate_service_cb> gatemap_;

 private:
    dj_direct_gatemap(const dj_direct_gatemap&);
    dj_direct_gatemap &operator=(const dj_direct_gatemap&);
};

#endif
