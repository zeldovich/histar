/*
 * RPC protocol layered on top of one-way message delivery.
 */

%#include <dj/dj.h>

struct dj_call_msg {
    unsigned hyper return_ct;
    dj_message_endpoint return_ep;
    opaque buf<>;
};

