#ifndef JOS_DJ_DJSRPC_HH
#define JOS_DJ_DJSRPC_HH

#include <dj/djrpc.hh>
#include <dj/djgatesrv.hh>
#include <dj/gatesender.hh>

// Client-side RPC interface
class dj_rpc_call {
 public:
    dj_rpc_call(gate_sender *gs)
	: gs_(gs) {}
    dj_delivery_code call(const dj_pubkey&, time_t timeout,
			  const dj_delegation_set&, const dj_catmap&,
			  const dj_message&, dj_message *reply);

 private:
    gate_sender *gs_;
};

// Server-side RPC handling
void dj_rpc_srv(dj_rpc_service_fn*, gate_call_data*, gatesrv_return*);

#endif
