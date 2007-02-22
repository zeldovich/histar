/*
 * Message format for incoming requests into djd's local gate.
 */

%#include <dj/djprotx.h>

struct dj_incoming_gate_req {
    dj_pubkey node;
    time_t timeout;
    dj_delegation_set dset;
    dj_catmap catmap;
    dj_message m;
};

union dj_incoming_gate_res switch (dj_delivery_code stat) {
 case DELIVERY_DONE:
    unsigned hyper token;
 default:
    void;
};

struct dj_outgoing_gate_msg {
    dj_pubkey sender;
    dj_gatename djd_gate;
    dj_message m;
};
