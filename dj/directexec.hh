#ifndef JOS_DJ_DIRECTEXEC_HH
#define JOS_DJ_DIRECTEXEC_HH

#include <qhash.h>
#include <dj/stuff.hh>

class dj_direct_gatemap : public dj_gate_factory {
 public:
    dj_direct_gatemap() : gatemap_(), counter_(1) {}
    void deliver(const dj_message&, const delivery_args&);
    virtual dj_message_endpoint create_gate(uint64_t container, dj_msg_sink);
    virtual void destroy(const dj_message_endpoint &ep);

 private:
    dj_direct_gatemap(const dj_direct_gatemap&);
    dj_direct_gatemap &operator=(const dj_direct_gatemap&);
    qhash<cobj_ref, dj_msg_sink> gatemap_;

    uint64_t counter_;
};

#endif
