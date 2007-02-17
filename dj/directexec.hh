#ifndef JOS_DJ_DIRECTEXEC_HH
#define JOS_DJ_DIRECTEXEC_HH

#include <qhash.h>
#include <dj/dis.hh>
#include <dj/djops.hh>

class dj_direct_gatemap {
 public:
    dj_direct_gatemap() : gatemap_(), token_(1) {}
    void deliver(const dj_message_endpoint&, const dj_message_args&,
		 djprot::delivery_status_cb);
    qhash<cobj_ref, dj_msg_sink> gatemap_;

 private:
    dj_direct_gatemap(const dj_direct_gatemap&);
    dj_direct_gatemap &operator=(const dj_direct_gatemap&);

    uint64_t token_;
};

#endif
