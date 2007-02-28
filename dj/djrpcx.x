/*
 * RPC protocol layered on top of one-way message delivery.
 */

%#include <dj/djprotx.h>

struct dj_call_msg {
    dj_message_endpoint return_ep;
    dj_catmap return_cm;
    dj_delegation_set return_ds;
    opaque buf<>;
};

