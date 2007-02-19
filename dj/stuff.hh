#ifndef JOS_DJ_STUFF_HH
#define JOS_DJ_STUFF_HH

#include <dj/djprot.hh>
#include <dj/djops.hh>

typedef callback<void, const dj_pubkey&, const dj_message&, uint64_t>::ptr dj_msg_sink;

class dj_gate_factory {
 public:
    virtual ~dj_gate_factory() {}
    virtual dj_message_endpoint create_gate(uint64_t ct, dj_msg_sink) = 0;
    virtual void destroy(const dj_message_endpoint&) = 0;
};

#endif
