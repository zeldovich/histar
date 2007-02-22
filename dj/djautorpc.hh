#ifndef JOS_DJ_DJAUTORPC_HH
#define JOS_DJ_DJAUTORPC_HH

#include <dj/djsrpc.hh>

class dj_autorpc {
 public:
    dj_autorpc(gate_sender *gs, time_t tmo, const dj_pubkey &pk,
	       uint64_t ct, dj_node_cache *home, dj_node_cache *nc)
	: gs_(gs), tmo_(tmo), pk_(pk), msgct_(ct), home_(home), nc_(nc) {}

    template<class TA, class TR>
    dj_delivery_code call(const dj_message_endpoint&, const TA&, TR&,
			  label *taint = 0,
			  label *grant = 0,
			  label *gclear = 0);

 private:
    gate_sender *gs_;
    time_t tmo_;
    dj_pubkey pk_;
    uint64_t msgct_;
    dj_node_cache *home_;
    dj_node_cache *nc_;
};

#endif
