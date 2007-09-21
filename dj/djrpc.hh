#ifndef JOS_DJ_DJRPC_HH
#define JOS_DJ_DJRPC_HH

#include <async.h>
#include <dj/djprotx.h>

struct dj_rpc_reply {
    dj_delegation_set dset;
    dj_catmap catmap;
    dj_message msg;
};

typedef bool (dj_rpc_service_fn)(const dj_message&, const str&, dj_rpc_reply*);
typedef callback<bool, const dj_message&, const str&,
		       dj_rpc_reply*>::ptr dj_rpc_service_cb;

// For debugging purposes.
bool dj_echo_service(const dj_message&, const str&, dj_rpc_reply*);

#endif
