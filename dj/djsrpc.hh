#ifndef JOS_DJ_DJSRPC_HH
#define JOS_DJ_DJSRPC_HH

#include <dj/djrpc.hh>
#include <dj/djgatesrv.hh>
#include <dj/gatesender.hh>

// Client-side RPC interface
dj_delivery_code dj_rpc_call(gate_sender*, time_t timeout,
			     const dj_delegation_set&, const dj_catmap&,
			     const dj_message&, const str&,
			     dj_message *reply, label *grantlabel,
			     label *return_ct_taint, bool gateret);

// Server-side RPC handling
void dj_rpc_srv(dj_rpc_service_fn*, gate_call_data*, gatesrv_return*);

#endif
